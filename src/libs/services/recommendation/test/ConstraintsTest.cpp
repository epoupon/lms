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

#include "track-selection-constraints/DuplicateTrackConstraint.hpp"
#include "track-selection-constraints/SameArtistConstraint.hpp"
#include "track-selection-constraints/SameReleaseConstraint.hpp"
#include "track-selection-constraints/TrackCandidateContext.hpp"
#include "track-selection-constraints/TrackCandidateEvaluator.hpp"
#include "track-selection-constraints/TrackMetadata.hpp"

using namespace lms;
using namespace lms::recommendation;

namespace
{
    const db::TrackId T1{ 1 };
    const db::TrackId T2{ 2 };
    const db::TrackId T3{ 3 };
    const db::TrackId T4{ 4 };
    const db::TrackId T5{ 5 };

    const db::ArtistId A1{ 10 };
    const db::ArtistId A2{ 20 };

    const db::ReleaseId R1{ 100 };
    const db::ReleaseId R2{ 200 };
} // namespace

TEST(DuplicateTrackConstraint, acceptsNewCandidate)
{
    const std::vector<db::TrackId> selected{ T1, T2 };
    const TrackCandidateContext ctx{ .candidateTrackId = T3, .selectedTracks = selected };
    EXPECT_FALSE(DuplicateTrackConstraint{}.rejects(ctx));
}

TEST(DuplicateTrackConstraint, rejectsAlreadySelected)
{
    const std::vector<db::TrackId> selected{ T1, T2, T3 };
    const TrackCandidateContext ctx{ .candidateTrackId = T2, .selectedTracks = selected };
    EXPECT_TRUE(DuplicateTrackConstraint{}.rejects(ctx));
}

TEST(DuplicateTrackConstraint, acceptsWhenSelectionEmpty)
{
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {} };
    EXPECT_FALSE(DuplicateTrackConstraint{}.rejects(ctx));
}

TEST(SameArtistConstraint, zeroScoreWhenNoSharedArtist)
{
    const TrackMetadataMap meta{
        { T1, { .releaseId = {}, .artistIds = { A1 } } },
        { T2, { .releaseId = {}, .artistIds = { A2 } } },
    };
    const SameArtistConstraint constraint{ meta };
    const std::vector<db::TrackId> selected{ T2 };
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected };
    EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
}

TEST(SameArtistConstraint, fullScoreWhenMostRecentMatchesArtist)
{
    TrackMetadataMap meta{
        { T1, { .releaseId = {}, .artistIds = { A1 } } },
        { T2, { .releaseId = {}, .artistIds = { A1 } } },
    };
    const SameArtistConstraint constraint{ meta };
    const std::vector<db::TrackId> selected{ T2 };
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected };

    EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 1.F);
}

TEST(SameArtistConstraint, halfScoreWhenSecondMostRecentMatchesArtist)
{
    TrackMetadataMap meta{
        { T1, { .releaseId = {}, .artistIds = { A1 } } },
        { T2, { .releaseId = {}, .artistIds = { A2 } } },
        { T3, { .releaseId = {}, .artistIds = { A1 } } },
    };
    const SameArtistConstraint constraint{ meta };

    const std::vector<db::TrackId> selected{ T3, T2 };
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected };

    EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.5F);
}

TEST(SameArtistConstraint, trackOutsideWindowIsIgnored)
{
    TrackMetadataMap meta{
        { T1, { .releaseId = {}, .artistIds = { A1 } } },
        { T2, { .releaseId = {}, .artistIds = { A2 } } },
        { T3, { .releaseId = {}, .artistIds = { A2 } } },
        { T4, { .releaseId = {}, .artistIds = { A2 } } },
        { T5, { .releaseId = {}, .artistIds = { A1 } } }, // outside window=4
    };
    const SameArtistConstraint constraint{ meta, /*window=*/4 };
    TrackMetadataMap meta2{
        { T1, { .releaseId = {}, .artistIds = { A1 } } },
        { T2, { .releaseId = {}, .artistIds = { A2 } } },
        { T3, { .releaseId = {}, .artistIds = { A1 } } },
    };
    const SameArtistConstraint constraint2{ meta2, /*window=*/1 };

    const std::vector<db::TrackId> selected{ T3, T2 };
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected };
    EXPECT_FLOAT_EQ(constraint2.computeScore(ctx), 0.F);
}

TEST(SameArtistConstraint, zeroScoreWhenCandidateNotInMap)
{
    const TrackMetadataMap meta{};
    const SameArtistConstraint constraint{ meta };
    const std::vector<db::TrackId> selected{ T2 };
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected };
    EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
}

TEST(SameReleaseConstraint, zeroScoreWhenNoSharedRelease)
{
    TrackMetadataMap meta{
        { T1, { .releaseId = R1, .artistIds = {} } },
        { T2, { .releaseId = R2, .artistIds = {} } },
    };
    const SameReleaseConstraint constraint{ meta };
    const std::vector<db::TrackId> selected{ T2 };
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected };
    EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
}

TEST(SameReleaseConstraint, fullScoreWhenMostRecentMatchesRelease)
{
    TrackMetadataMap meta{
        { T1, { .releaseId = R1, .artistIds = {} } },
        { T2, { .releaseId = R1, .artistIds = {} } },
    };
    const SameReleaseConstraint constraint{ meta };
    const std::vector<db::TrackId> selected{ T2 };
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected };
    EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 1.F);
}

TEST(SameReleaseConstraint, zeroScoreWhenCandidateHasNoRelease)
{
    TrackMetadataMap meta{
        { T1, { .releaseId = {}, .artistIds = {} } },
        { T2, { .releaseId = R1, .artistIds = {} } },
    };
    const SameReleaseConstraint constraint{ meta };
    const std::vector<db::TrackId> selected{ T2 };
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected };
    EXPECT_FLOAT_EQ(constraint.computeScore(ctx), 0.F);
}

TEST(TrackCandidateEvaluator, hardConstraintRejects)
{
    TrackCandidateEvaluator evaluator;
    evaluator.addHardConstraint(std::make_unique<DuplicateTrackConstraint>());

    const std::vector<db::TrackId> selected{ T1 };
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected };
    EXPECT_TRUE(evaluator.rejects(ctx));
}

TEST(TrackCandidateEvaluator, noHardConstraintDoesNotReject)
{
    TrackCandidateEvaluator evaluator;
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = {} };
    EXPECT_FALSE(evaluator.rejects(ctx));
}

TEST(TrackCandidateEvaluator, softConstraintScoreIsWeighted)
{
    TrackMetadataMap meta{
        { T1, { .releaseId = R1, .artistIds = {} } },
        { T2, { .releaseId = R1, .artistIds = {} } },
    };
    TrackCandidateEvaluator evaluator;
    evaluator.addSoftConstraint(std::make_unique<SameReleaseConstraint>(meta), 2.F);

    const std::vector<db::TrackId> selected{ T2 };
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected };

    EXPECT_FLOAT_EQ(evaluator.score(ctx), 2.F);
}

TEST(TrackCandidateEvaluator, multipleSoftConstraintsAreAccumulated)
{
    TrackMetadataMap meta{
        { T1, { .releaseId = R1, .artistIds = { A1 } } },
        { T2, { .releaseId = R1, .artistIds = { A1 } } },
    };
    TrackCandidateEvaluator evaluator;
    evaluator.addSoftConstraint(std::make_unique<SameReleaseConstraint>(meta), 1.F);
    evaluator.addSoftConstraint(std::make_unique<SameArtistConstraint>(meta), 1.F);

    const std::vector<db::TrackId> selected{ T2 };
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected };

    EXPECT_FLOAT_EQ(evaluator.score(ctx), 2.F);
}

TEST(TrackCandidateEvaluator, hardConstraintPassesEvenWithSoftConstraints)
{
    TrackMetadataMap meta{
        { T1, { .releaseId = R1, .artistIds = {} } },
        { T2, { .releaseId = R1, .artistIds = {} } },
    };
    TrackCandidateEvaluator evaluator;
    evaluator.addHardConstraint(std::make_unique<DuplicateTrackConstraint>());
    evaluator.addSoftConstraint(std::make_unique<SameReleaseConstraint>(meta), 1.F);

    const std::vector<db::TrackId> selected{ T2 };
    const TrackCandidateContext ctx{ .candidateTrackId = T1, .selectedTracks = selected };
    EXPECT_FALSE(evaluator.rejects(ctx));
    EXPECT_FLOAT_EQ(evaluator.score(ctx), 1.F);
}
