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

#include <cmath>
#include <functional>
#include <unordered_set>

#include <gtest/gtest.h>

#include "som/Matrix.hpp"

namespace lms::som
{
    class MatrixTest : public ::testing::Test
    {
    protected:
        static constexpr Coordinate Width{ 5 };
        static constexpr Coordinate Height{ 3 };
    };

    TEST_F(MatrixTest, DefaultConstruction)
    {
        Matrix<int> matrix;

        EXPECT_EQ(matrix.getWidth(), 0);
        EXPECT_EQ(matrix.getHeight(), 0);
    }

    TEST_F(MatrixTest, ConstructorWidthHeight)
    {
        Matrix<int> matrix{ Width, Height };

        EXPECT_EQ(matrix.getWidth(), Width);
        EXPECT_EQ(matrix.getHeight(), Height);

        for (Coordinate y{}; y < matrix.getHeight(); ++y)
        {
            for (Coordinate x{}; x < matrix.getWidth(); ++x)
                EXPECT_EQ(matrix.get(x, y), 0);
        }
    }

    TEST_F(MatrixTest, ConstructorWithInitialValue)
    {
        Matrix<int> matrix{ Width, Height, 7 };

        for (Coordinate y{}; y < matrix.getHeight(); ++y)
        {
            for (Coordinate x{}; x < matrix.getWidth(); ++x)
                EXPECT_EQ(matrix.get(x, y), 7);
        }
    }

    TEST_F(MatrixTest, Resize)
    {
        Matrix<int> matrix(Width, Height, 42);
        matrix.resize(Width * 2, Height * 2, 7);

        for (Coordinate y{}; y < matrix.getHeight(); ++y)
        {
            for (Coordinate x{}; x < matrix.getWidth(); ++x)
                EXPECT_EQ(matrix.get(x, y), 7);
        }
    }

    TEST_F(MatrixTest, ResizeDefaultValue)
    {
        Matrix<int> matrix(Width, Height, 42);
        matrix.resize(Width * 2, Height * 2);

        for (Coordinate y{}; y < matrix.getHeight(); ++y)
        {
            for (Coordinate x{}; x < matrix.getWidth(); ++x)
                EXPECT_EQ(matrix.get(x, y), 0);
        }
    }

    TEST_F(MatrixTest, ElementAccessMutableAndConst)
    {
        Matrix<int> matrix{ Width, Height };
        matrix[{ 1, 0 }] = 42;
        matrix[{ 2, 1 }] = 99;

        EXPECT_EQ(matrix.get(1, 0), 42);
        EXPECT_EQ(matrix.get(2, 1), 99);

        const Matrix<int>& constMatrix{ matrix };
        EXPECT_EQ(constMatrix.get(1, 0), 42);
        EXPECT_EQ(constMatrix.get(2, 1), 99);
    }

    TEST_F(MatrixTest, Fill)
    {
        Matrix<int> matrix{ Width, Height, 5 };
        matrix.fill(42);

        for (Coordinate y{}; y < matrix.getHeight(); ++y)
        {
            for (Coordinate x{}; x < matrix.getWidth(); ++x)
                EXPECT_EQ(matrix.get(x, y), 42);
        }
    }

    TEST_F(MatrixTest, getPositionBestScore)
    {
        Matrix<int> matrix{ 2, 2 };
        matrix[{ 0, 0 }] = 3;
        matrix[{ 1, 0 }] = 1;
        matrix[{ 0, 1 }] = 6;
        matrix[{ 1, 1 }] = 4;

        const MatrixPosition expected{ 1, 1 };
        EXPECT_EQ(matrix.getPositionMinDistance([](int value) -> float {
            return std::abs(value - 4);
        }),
                  expected);
    }

    TEST_F(MatrixTest, MatrixPositionHashIsValid)
    {
        std::unordered_set<MatrixPosition> positions;
        const MatrixPosition position{ 3, 2 };

        positions.insert(position);
        EXPECT_NE(positions.find(position), positions.end());
    }
} // namespace lms::som
