/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <gtest/gtest.h>

#include "database/objects/ArtistId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "math/Vector.hpp"

#include "core/UUID.hpp"

#include "audio-similarity/InterpolationFitConstraint.hpp"
#include "audio-similarity/MaxDistanceConstraint.hpp"
#include "audio-similarity/NearDuplicateEmbeddingConstraint.hpp"
#include "audio-similarity/SmoothTransitionConstraint.hpp"
#include "track-selection-constraints/DuplicateTrackConstraint.hpp"
#include "track-selection-constraints/SameArtistConstraint.hpp"
#include "track-selection-constraints/SameRecordingMBIDConstraint.hpp"
#include "track-selection-constraints/SameReleaseConstraint.hpp"
#include "track-selection-constraints/TrackCandidateContext.hpp"
#include "track-selection-constraints/TrackCandidateEvaluator.hpp"
#include "track-selection-constraints/TrackMetadata.hpp"

namespace lms::recommendation::tests
{
    namespace
    {
        const db::TrackId T1{ 1 };
        const db::TrackId T2{ 2 };
        const db::TrackId T3{ 3 };
        const db::TrackId T4{ 4 };

        const db::ArtistId A1{ 10 };
        const db::ArtistId A2{ 20 };

        const db::ReleaseId R1{ 100 };
        const db::ReleaseId R2{ 200 };
    } // namespace

    TEST(DuplicateTrackConstraint, acceptsNewCandidate)
    {
        const std::vector<db::TrackId> selected{ T1, T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T3, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FALSE(DuplicateTrackConstraint{}.rejects(ctx));
    }

    TEST(DuplicateTrackConstraint, rejectsAlreadySelected)
    {
        const std::vector<db::TrackId> selected{ T1, T2, T3 };
        const TrackCandidateContext ctx{ .candidateTrackId = T2, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_TRUE(DuplicateTrackConstraint{}.rejects(ctx));
    }

    TEST(DuplicateTrackConstraint, acceptsWhenSelectionEmpty)
    {
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {}, .seedTrackIds = {} };
        EXPECT_FALSE(DuplicateTrackConstraint{}.rejects(ctx));
    }

    TEST(SameArtistConstraint, zeroScoreWhenNoSharedArtist)
    {
        const TrackMetadataMap meta{
            { T1, { .releaseId = {}, .artistIds = { A1 }, .recordingMBID = {} } },
            { T2, { .releaseId = {}, .artistIds = { A2 }, .recordingMBID = {} } },
        };
        const SameArtistConstraint constraint{ meta };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
    }

    TEST(SameArtistConstraint, fullScoreWhenMostRecentMatchesArtist)
    {
        TrackMetadataMap meta{
            { T1, { .releaseId = {}, .artistIds = { A1 }, .recordingMBID = {} } },
            { T2, { .releaseId = {}, .artistIds = { A1 }, .recordingMBID = {} } },
        };
        const SameArtistConstraint constraint{ meta };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };

        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 1.F);
    }

    TEST(SameArtistConstraint, halfScoreWhenSecondMostRecentMatchesArtist)
    {
        TrackMetadataMap meta{
            { T1, { .releaseId = {}, .artistIds = { A1 }, .recordingMBID = {} } },
            { T2, { .releaseId = {}, .artistIds = { A2 }, .recordingMBID = {} } },
            { T3, { .releaseId = {}, .artistIds = { A1 }, .recordingMBID = {} } },
        };
        const SameArtistConstraint constraint{ meta };

        const std::vector<db::TrackId> selected{ T3, T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };

        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.5F);
    }

    TEST(SameArtistConstraint, trackOutsideWindowIsIgnored)
    {
        TrackMetadataMap meta{
            { T1, { .releaseId = {}, .artistIds = { A1 }, .recordingMBID = {} } },
            { T2, { .releaseId = {}, .artistIds = { A2 }, .recordingMBID = {} } },
            { T3, { .releaseId = {}, .artistIds = { A1 }, .recordingMBID = {} } },
        };
        const SameArtistConstraint constraint{ meta, /*window=*/1 };

        const std::vector<db::TrackId> selected{ T3, T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
    }

    TEST(SameArtistConstraint, zeroScoreWhenCandidateNotInMap)
    {
        const TrackMetadataMap meta{};
        const SameArtistConstraint constraint{ meta };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
    }

    TEST(SameReleaseConstraint, zeroScoreWhenNoSharedRelease)
    {
        TrackMetadataMap meta{
            { T1, { .releaseId = R1, .artistIds = {}, .recordingMBID = {} } },
            { T2, { .releaseId = R2, .artistIds = {}, .recordingMBID = {} } },
        };
        const SameReleaseConstraint constraint{ meta };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
    }

    TEST(SameReleaseConstraint, fullScoreWhenMostRecentMatchesRelease)
    {
        TrackMetadataMap meta{
            { T1, { .releaseId = R1, .artistIds = {}, .recordingMBID = {} } },
            { T2, { .releaseId = R1, .artistIds = {}, .recordingMBID = {} } },
        };
        const SameReleaseConstraint constraint{ meta };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 1.F);
    }

    TEST(SameReleaseConstraint, zeroScoreWhenCandidateHasNoRelease)
    {
        TrackMetadataMap meta{
            { T1, { .releaseId = {}, .artistIds = {}, .recordingMBID = {} } },
            { T2, { .releaseId = R1, .artistIds = {}, .recordingMBID = {} } },
        };
        const SameReleaseConstraint constraint{ meta };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
    }

    TEST(TrackCandidateEvaluator, hardConstraintRejects)
    {
        TrackCandidateEvaluator evaluator;
        evaluator.addHardConstraint(std::make_unique<DuplicateTrackConstraint>());

        const std::vector<db::TrackId> selected{ T1 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_TRUE(evaluator.rejects(ctx));
    }

    TEST(TrackCandidateEvaluator, noHardConstraintDoesNotReject)
    {
        TrackCandidateEvaluator evaluator;
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {}, .seedTrackIds = {} };
        EXPECT_FALSE(evaluator.rejects(ctx));
    }

    TEST(TrackCandidateEvaluator, softConstraintScoreIsWeighted)
    {
        TrackMetadataMap meta{
            { T1, { .releaseId = R1, .artistIds = {}, .recordingMBID = {} } },
            { T2, { .releaseId = R1, .artistIds = {}, .recordingMBID = {} } },
        };
        TrackCandidateEvaluator evaluator;
        evaluator.addSoftConstraint(std::make_unique<SameReleaseConstraint>(meta), 2.F);

        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };

        EXPECT_FLOAT_EQ(evaluator.score(ctx), 2.F);
    }

    TEST(TrackCandidateEvaluator, multipleSoftConstraintsAreAccumulated)
    {
        TrackMetadataMap meta{
            { T1, { .releaseId = R1, .artistIds = { A1 }, .recordingMBID = {} } },
            { T2, { .releaseId = R1, .artistIds = { A1 }, .recordingMBID = {} } },
        };
        TrackCandidateEvaluator evaluator;
        evaluator.addSoftConstraint(std::make_unique<SameReleaseConstraint>(meta), 1.F);
        evaluator.addSoftConstraint(std::make_unique<SameArtistConstraint>(meta), 1.F);

        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };

        EXPECT_FLOAT_EQ(evaluator.score(ctx), 2.F);
    }

    TEST(TrackCandidateEvaluator, hardConstraintPassesEvenWithSoftConstraints)
    {
        TrackMetadataMap meta{
            { T1, { .releaseId = R1, .artistIds = {}, .recordingMBID = {} } },
            { T2, { .releaseId = R1, .artistIds = {}, .recordingMBID = {} } },
        };
        TrackCandidateEvaluator evaluator;
        evaluator.addHardConstraint(std::make_unique<DuplicateTrackConstraint>());
        evaluator.addSoftConstraint(std::make_unique<SameReleaseConstraint>(meta), 1.F);

        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FALSE(evaluator.rejects(ctx));
        EXPECT_FLOAT_EQ(evaluator.score(ctx), 1.F);
    }

    namespace
    {
        using TestVec = math::Vector<2, float>;
        const TestVec v_x{ 1.F, 0.F };
        const TestVec v_y{ 0.F, 1.F };

        using TestConstraint = NearDuplicateEmbeddingConstraint<2>;
        using TestVectorMap = TestConstraint::TrackVectorMap;
    } // namespace

    TEST(NearDuplicateEmbeddingConstraint, acceptsWhenCandidateNotInMap)
    {
        const TestVectorMap trackVectors{ { T1, &v_x } };
        const TestConstraint constraint{ trackVectors, 0.1F };
        const std::vector<db::TrackId> selected{ T1 };
        const TrackCandidateContext ctx{ .candidateTrackId = T2, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FALSE(constraint.rejects(ctx));
    }

    TEST(NearDuplicateEmbeddingConstraint, acceptsWhenSelectionEmpty)
    {
        const TestVectorMap trackVectors{ { T1, &v_x } };
        const TestConstraint constraint{ trackVectors, 0.1F };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {}, .seedTrackIds = {} };
        EXPECT_FALSE(constraint.rejects(ctx));
    }

    TEST(NearDuplicateEmbeddingConstraint, acceptsWhenSelectedTrackNotInMap)
    {
        const TestVectorMap trackVectors{ { T1, &v_x } };
        const TestConstraint constraint{ trackVectors, 0.1F };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FALSE(constraint.rejects(ctx));
    }

    TEST(NearDuplicateEmbeddingConstraint, rejectsWhenDistanceBelowThreshold)
    {
        const TestVectorMap trackVectors{ { T1, &v_x }, { T2, &v_x } };
        const TestConstraint constraint{ trackVectors, 0.1F };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_TRUE(constraint.rejects(ctx));
    }

    TEST(NearDuplicateEmbeddingConstraint, acceptsWhenDistanceAboveThreshold)
    {
        const TestVectorMap trackVectors{ { T1, &v_x }, { T2, &v_y } };
        const TestConstraint constraint{ trackVectors, 0.1F };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FALSE(constraint.rejects(ctx));
    }

    TEST(NearDuplicateEmbeddingConstraint, rejectsWhenOneOfManySelectedIsNearDuplicate)
    {
        const TestVectorMap trackVectors{ { T1, &v_x }, { T3, &v_y }, { T4, &v_x } };
        const TestConstraint constraint{ trackVectors, 0.1F };
        const std::vector<db::TrackId> selected{ T3, T4 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_TRUE(constraint.rejects(ctx));
    }

    TEST(NearDuplicateEmbeddingConstraint, acceptsWhenAllSelectedAreFarEnough)
    {
        const TestVectorMap trackVectors{ { T1, &v_x }, { T2, &v_y }, { T3, &v_y } };
        const TestConstraint constraint{ trackVectors, 0.1F };
        const std::vector<db::TrackId> selected{ T2, T3 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FALSE(constraint.rejects(ctx));
    }

    TEST(InterpolationFitConstraint, zeroScoreWhenNoSeeds)
    {
        const TestVectorMap trackVectors{ { T1, &v_x } };
        const InterpolationFitConstraint<2> constraint{ trackVectors };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {}, .seedTrackIds = {} };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
    }

    TEST(InterpolationFitConstraint, zeroScoreWhenCandidateMatchesSeed)
    {
        const TestVectorMap trackVectors{ { T1, &v_x }, { T2, &v_x } };
        const InterpolationFitConstraint<2> constraint{ trackVectors };
        const std::vector<db::TrackId> seeds{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {}, .seedTrackIds = seeds };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
    }

    TEST(InterpolationFitConstraint, halfScoreWhenCandidateOrthogonalToSeed)
    {
        const TestVectorMap trackVectors{ { T1, &v_x }, { T2, &v_y } };
        const InterpolationFitConstraint<2> constraint{ trackVectors };
        const std::vector<db::TrackId> seeds{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {}, .seedTrackIds = seeds };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.5F);
    }

    TEST(InterpolationFitConstraint, usesClosestSeed)
    {
        const TestVectorMap trackVectors{ { T1, &v_x }, { T2, &v_x }, { T3, &v_y } };
        const InterpolationFitConstraint<2> constraint{ trackVectors };
        const std::vector<db::TrackId> seeds{ T3, T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {}, .seedTrackIds = seeds };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
    }

    TEST(MaxDistanceConstraint, acceptsWhenNoSeeds)
    {
        const TestVectorMap trackVectors{ { T1, &v_x } };
        const MaxDistanceConstraint<2> constraint{ trackVectors, 0.5F };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {}, .seedTrackIds = {} };
        EXPECT_FALSE(constraint.rejects(ctx));
    }

    TEST(MaxDistanceConstraint, acceptsWhenWithinThresholdOfOneSeed)
    {
        const TestVectorMap trackVectors{ { T1, &v_x }, { T2, &v_x } };
        const MaxDistanceConstraint<2> constraint{ trackVectors, 0.5F };
        const std::vector<db::TrackId> seeds{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {}, .seedTrackIds = seeds };
        EXPECT_FALSE(constraint.rejects(ctx));
    }

    TEST(MaxDistanceConstraint, rejectsWhenBeyondThresholdFromAllSeeds)
    {
        const TestVectorMap trackVectors{ { T1, &v_x }, { T2, &v_y } };
        const MaxDistanceConstraint<2> constraint{ trackVectors, 0.1F };
        const std::vector<db::TrackId> seeds{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {}, .seedTrackIds = seeds };
        EXPECT_TRUE(constraint.rejects(ctx));
    }

    TEST(MaxDistanceConstraint, acceptsWhenAnyOfManySeedsIsCloseEnough)
    {
        const TestVectorMap trackVectors{ { T1, &v_x }, { T2, &v_y }, { T3, &v_x } };
        const MaxDistanceConstraint<2> constraint{ trackVectors, 0.5F };
        const std::vector<db::TrackId> seeds{ T2, T3 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {}, .seedTrackIds = seeds };
        EXPECT_FALSE(constraint.rejects(ctx));
    }

    TEST(SmoothTransitionConstraint, zeroScoreWhenNoSelectedTracks)
    {
        const TestVectorMap trackVectors{ { T1, &v_x } };
        const SmoothTransitionConstraint<2> constraint{ trackVectors };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {}, .seedTrackIds = {} };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
    }

    TEST(SmoothTransitionConstraint, zeroScoreWhenPreviousMatchesCandidate)
    {
        const TestVectorMap trackVectors{ { T1, &v_x }, { T2, &v_x } };
        const SmoothTransitionConstraint<2> constraint{ trackVectors };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
    }

    TEST(SmoothTransitionConstraint, halfScoreWhenPreviousOrthogonalToCandidate)
    {
        const TestVectorMap trackVectors{ { T1, &v_x }, { T2, &v_y } };
        const SmoothTransitionConstraint<2> constraint{ trackVectors };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.5F);
    }

    TEST(SmoothTransitionConstraint, usesMostRecentlySelectedTrack)
    {
        const TestVectorMap trackVectors{ { T1, &v_x }, { T2, &v_x }, { T3, &v_y } };
        const SmoothTransitionConstraint<2> constraint{ trackVectors };
        const std::vector<db::TrackId> selected{ T3, T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
    }

    namespace
    {
        const core::UUID MBID1{ *core::UUID::fromString("3d2508f7-3a7e-4f25-895c-fca079dd71ce") };
        const core::UUID MBID2{ *core::UUID::fromString("8e44e4ea-9bce-47c2-a48c-8aa338242fb2") };
    } // namespace

    TEST(SameRecordingMBIDConstraint, acceptsWhenCandidateNotInMap)
    {
        const TrackMetadataMap meta{ { T2, { .releaseId = {}, .artistIds = {}, .recordingMBID = MBID1 } } };
        const SameRecordingMBIDConstraint constraint{ meta };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FALSE(constraint.rejects(ctx));
    }

    TEST(SameRecordingMBIDConstraint, acceptsWhenCandidateHasNoMBID)
    {
        const TrackMetadataMap meta{
            { T1, { .releaseId = {}, .artistIds = {}, .recordingMBID = std::nullopt } },
            { T2, { .releaseId = {}, .artistIds = {}, .recordingMBID = MBID1 } },
        };
        const SameRecordingMBIDConstraint constraint{ meta };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FALSE(constraint.rejects(ctx));
    }

    TEST(SameRecordingMBIDConstraint, acceptsWhenSelectionEmpty)
    {
        const TrackMetadataMap meta{ { T1, { .releaseId = {}, .artistIds = {}, .recordingMBID = MBID1 } } };
        const SameRecordingMBIDConstraint constraint{ meta };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {}, .seedTrackIds = {} };
        EXPECT_FALSE(constraint.rejects(ctx));
    }

    TEST(SameRecordingMBIDConstraint, acceptsWhenNoneOfSelectedHasSameMBID)
    {
        const TrackMetadataMap meta{
            { T1, { .releaseId = {}, .artistIds = {}, .recordingMBID = MBID1 } },
            { T2, { .releaseId = {}, .artistIds = {}, .recordingMBID = MBID2 } },
        };
        const SameRecordingMBIDConstraint constraint{ meta };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FALSE(constraint.rejects(ctx));
    }

    TEST(SameRecordingMBIDConstraint, rejectsWhenOneOfSelectedHasSameMBID)
    {
        const TrackMetadataMap meta{
            { T1, { .releaseId = {}, .artistIds = {}, .recordingMBID = MBID1 } },
            { T2, { .releaseId = {}, .artistIds = {}, .recordingMBID = MBID2 } },
            { T3, { .releaseId = {}, .artistIds = {}, .recordingMBID = MBID1 } },
        };
        const SameRecordingMBIDConstraint constraint{ meta };
        const std::vector<db::TrackId> selected{ T2, T3 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_TRUE(constraint.rejects(ctx));
    }

    TEST(SameRecordingMBIDConstraint, acceptsWhenSelectedTrackNotInMap)
    {
        const TrackMetadataMap meta{ { T1, { .releaseId = {}, .artistIds = {}, .recordingMBID = MBID1 } } };
        const SameRecordingMBIDConstraint constraint{ meta };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FALSE(constraint.rejects(ctx));
    }

    TEST(SameRecordingMBIDConstraint, acceptsWhenSelectedTrackHasNoMBID)
    {
        const TrackMetadataMap meta{
            { T1, { .releaseId = {}, .artistIds = {}, .recordingMBID = MBID1 } },
            { T2, { .releaseId = {}, .artistIds = {}, .recordingMBID = std::nullopt } },
        };
        const SameRecordingMBIDConstraint constraint{ meta };
        const std::vector<db::TrackId> selected{ T2 };
        const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected, .seedTrackIds = {} };
        EXPECT_FALSE(constraint.rejects(ctx));
    }
} // namespace lms::recommendation::tests