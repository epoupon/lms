/*
 * Copyright (C) 2019 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <numeric>

#include "utils/Random.hpp"

#include "ParallelFor.hpp"

template<typename Individual>
class GeneticAlgorithm
{
	public:
		using Score = float;

		using BreedFunction = std::function<Individual(const Individual&, const Individual&)>;
		using MutateFunction = std::function<void(Individual&)>;
		using ScoreFunction = std::function<Score(const Individual&)>;

		struct Params
		{
			std::size_t		nbWorkers {1};
			std::size_t		nbGenerations;
			float			crossoverRatio {0.5};
			float			mutationProbability {0.05};
			BreedFunction	breedFunction;
			MutateFunction		mutateFunction;
			ScoreFunction		scoreFunction;
		};

		GeneticAlgorithm(const Params& params);

		// Returns the individual that has the maximum score after processing the requested generations
		Individual simulate(const std::vector<Individual>& initialPopulation);

	private:
		struct ScoredIndividual
		{
			Individual individual;
			std::optional<Score> score {};
		};

		void scoreAndSortPopulation(std::vector<ScoredIndividual>& population);
		Score getTotalScore(const std::vector<ScoredIndividual>& population) const;
		typename std::vector<ScoredIndividual>::const_iterator pickRandomRouletteWheel(const std::vector<ScoredIndividual>& population, Score totalScore);

		Params _params;
};

template<typename Individual>
GeneticAlgorithm<Individual>::GeneticAlgorithm(const Params& params)
: _params {params}
{
}


template<typename Individual>
Individual
GeneticAlgorithm<Individual>::simulate(const std::vector<Individual>& initialPopulation)
{
	const std::size_t childrenCountPerGeneration {static_cast<std::size_t>(initialPopulation.size() * _params.crossoverRatio)};
	if (initialPopulation.size() < 10)
		throw std::runtime_error("Initial population must has at least 10 elements");

	std::vector<ScoredIndividual> scoredPopulation;
	scoredPopulation.reserve(initialPopulation.size());

	std::transform(std::cbegin(initialPopulation), std::cend(initialPopulation), std::back_inserter(scoredPopulation ),
			[](const Individual& individual) { return ScoredIndividual {individual};});

	scoreAndSortPopulation(scoredPopulation);

	for (std::size_t currentGeneration {}; currentGeneration  < _params.nbGenerations; ++currentGeneration)
	{
		assert(scoredPopulation.size() == initialPopulation.size());
		std::cout << "Processing generation " << currentGeneration << "..." << std::endl;
		std::cout << "Need to create " << childrenCountPerGeneration << " new children" << std::endl;

		// breed
		const Score populationTotalScore {getTotalScore(scoredPopulation)};
		std::vector<ScoredIndividual> children;
		children.reserve(childrenCountPerGeneration);

		while (children.size() < childrenCountPerGeneration)
		{
			// Select two random parents using their score as weight
			const auto itParent1 {pickRandomRouletteWheel(scoredPopulation, populationTotalScore)};
			const auto itParent2 {pickRandomRouletteWheel(scoredPopulation, populationTotalScore)};

			if (itParent1 == itParent2)
				continue;

			ScoredIndividual child {_params.breedFunction(itParent1->individual, itParent2->individual)};
			
			if (Random::getRealRandom(float {}, float {1}) <= _params.mutationProbability)
				_params.mutateFunction(child.individual);

			children.emplace_back(std::move(child));
		}

		// Elitist selection
		scoredPopulation.resize(initialPopulation.size() - childrenCountPerGeneration);

		scoredPopulation.insert(std::end(scoredPopulation), std::make_move_iterator(std::begin(children)), std::make_move_iterator(std::end(children)));
		assert(scoredPopulation.size() == initialPopulation.size());

		scoreAndSortPopulation(scoredPopulation);

		std::cout << "Mean score = " << getTotalScore(scoredPopulation) / scoredPopulation.size() << std::endl;
		std::cout << "Current best score = " << *scoredPopulation.front().score << std::endl;
	}

	std::cout << "Best score = " << *scoredPopulation.front().score << std::endl;
	return scoredPopulation.front().individual;
}


template<typename Individual>
void
GeneticAlgorithm<Individual>::scoreAndSortPopulation(std::vector<ScoredIndividual>& scoredPopulation)
{
	parallel_foreach(_params.nbWorkers, std::begin(scoredPopulation), std::end(scoredPopulation),
			[&](ScoredIndividual& scoredIndividual)
			{
				if (!scoredIndividual.score)
					scoredIndividual.score = _params.scoreFunction(scoredIndividual.individual);
			});

	std::sort(std::begin(scoredPopulation), std::end(scoredPopulation), [](const ScoredIndividual& a, const ScoredIndividual& b) { return a.score > b.score; });
}

template<typename Individual>
typename GeneticAlgorithm<Individual>::Score
GeneticAlgorithm<Individual>::getTotalScore(const std::vector<ScoredIndividual>& scoredPopulation) const
{
	return std::accumulate(std::cbegin(scoredPopulation), std::cend(scoredPopulation), Score {}, [](Score score, const ScoredIndividual& individual) { return score + *individual.score; });
}

template<typename Individual>
typename std::vector<typename GeneticAlgorithm<Individual>::ScoredIndividual>::const_iterator
GeneticAlgorithm<Individual>::pickRandomRouletteWheel(const std::vector<ScoredIndividual>& population, Score totalScore)
{
	const Score randomScore {Random::getRealRandom(Score {}, totalScore)};

	Score curScore{};
	for (auto itScoredIndividual {std::cbegin(population)}; itScoredIndividual != std::cend(population); ++itScoredIndividual )
	{
		if (curScore + *itScoredIndividual->score > randomScore)
			return itScoredIndividual;

		curScore += *itScoredIndividual->score;
	}

	throw std::runtime_error("bad random or empty population");
}


