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

#include <gtest/gtest.h>

#include "core/Service.hpp"

namespace lms::core::tests
{
    class IMyService
    {
    };

    class MyService : public IMyService
    {
    };

    class MyOtherService : public IMyService
    {
    };

    class MyServiceTag
    {
    };
    class MyOtherServiceTag
    {
    };

    TEST(Service, ctr)
    {
        EXPECT_FALSE(Service<IMyService>().exists());
        EXPECT_EQ(Service<IMyService>().get(), nullptr);

        Service<IMyService> myService{ std::make_unique<MyService>() };

        EXPECT_TRUE(Service<IMyService>().exists());
        EXPECT_EQ(Service<IMyService>().get(), myService.get());
    }

    TEST(Service, tags)
    {
        Service<IMyService, MyServiceTag> myService{ std::make_unique<MyService>() };
        Service<IMyService, MyOtherServiceTag> myOtherService{ std::make_unique<MyOtherService>() };

        EXPECT_FALSE(Service<IMyService>().exists());
        EXPECT_EQ(Service<IMyService>().get(), nullptr);

        EXPECT_TRUE((Service<IMyService, MyServiceTag>().exists()));
        EXPECT_TRUE((Service<IMyService, MyOtherServiceTag>().exists()));
        EXPECT_EQ((Service<IMyService, MyServiceTag>().get()), myService.get());
        EXPECT_EQ((Service<IMyService, MyOtherServiceTag>().get()), myOtherService.get());
    }
} // namespace lms::core::tests