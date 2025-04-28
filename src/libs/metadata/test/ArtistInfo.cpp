
/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "metadata/ArtistInfo.hpp"

namespace lms::metadata::tests
{
    TEST(ArtistInfo, basic)
    {
        std::istringstream is{ R"(<?xml version='1.0' encoding='UTF-8' standalone='yes'?>
<artist>
  <name>Tim Taylor</name>
  <musicBrainzArtistID>38811c52-85e3-4e2e-3319-ab7d9f2cfa5b</musicBrainzArtistID>
  <sortname>Taylor, Tim</sortname>
  <disambiguation>Timothy Taylor</disambiguation>
  <biography>DJ and producer based in London, UK. Founder of Missile Records and Planet Of Drums.&#13;
&#13;
He moved from the UK to Montreal in 1984 to become resident DJ at a number of clubs. In 1987, he began working as an A&amp;R for JSE Agency &amp; Management in New York, managing the likes of Tommy Musto, Frankie Bones, and The KLF. He also arranged and was tour manager for artists such as Womack &amp; Womack, Jungle Brothers, Ice-T, and Guru Josh.</biography>
  <thumb spoof="" cache="" aspect="thumb" preview="https://i.discogs.com/mG5pml7VOsld5ix_X_GNlY-wiN6axOjpFD4eZBkszL0/rs:fit/g:sm/q:40/h:150/w:150/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNi01/MTAwLmpwZWc.jpeg">https://i.discogs.com/zY8kWeJfDfWgDDJZ44uYARjNEzDLLqRiXk23LUlik-c/rs:fit/g:sm/q:90/h:800/w:600/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNi01/MTAwLmpwZWc.jpeg</thumb>
  <thumb spoof="" cache="" aspect="thumb" preview="https://i.discogs.com/AOk30C5KRPIub0DW8Q_3NP-PhtE0l3caXV1_r0lP3ao/rs:fit/g:sm/q:40/h:150/w:150/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTEyMTg2NTk2MC5q/cGc.jpeg">https://i.discogs.com/7Do2Xbok8HnWJEjcW6b0u9hyYMpNleGY3HRIEhNlxlM/rs:fit/g:sm/q:90/h:387/w:281/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTEyMTg2NTk2MC5q/cGc.jpeg</thumb>
  <thumb spoof="" cache="" aspect="thumb" preview="https://i.discogs.com/6YyKIXiD5wNTDVzS7JxpnipMUQ5UoJmtQgzhgV0-RB0/rs:fit/g:sm/q:40/h:150/w:150/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNi05/NDc3LmpwZWc.jpeg">https://i.discogs.com/fOeq1muY2Cu-gAJZGo5yK0AHIS1PJ1rcWqu8p_e3opY/rs:fit/g:sm/q:90/h:399/w:600/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNi05/NDc3LmpwZWc.jpeg</thumb>
  <thumb spoof="" cache="" aspect="thumb" preview="https://i.discogs.com/Ubw6Imsd8FUqoQnAb3VVbIqnh1b8VJDKAclTe2X7cXw/rs:fit/g:sm/q:40/h:150/w:150/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNS05/NzQ3LmpwZWc.jpeg">https://i.discogs.com/vhMFP7ICq7VyJcZaGim2X0x4nfKNGXkk7U5u153owfs/rs:fit/g:sm/q:90/h:540/w:364/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNS05/NzQ3LmpwZWc.jpeg</thumb>
  <thumb spoof="" cache="" aspect="thumb" preview="https://i.discogs.com/gXRlEh7W0awsBO3Cndww_n46JNLycyI6EOWajsBOU0A/rs:fit/g:sm/q:40/h:150/w:150/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNS03/NDYwLmpwZWc.jpeg">https://i.discogs.com/wsJi9gfDDaoamUjsxFm0R02VAllhW4iaFsCnwVfouO4/rs:fit/g:sm/q:90/h:399/w:600/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNS03/NDYwLmpwZWc.jpeg</thumb>
  <thumb spoof="" cache="" aspect="thumb" preview="https://i.discogs.com/QbSO414VlPwlRLcBAvhe6NFxCcsdy1rQkAmCzQ8Xe_o/rs:fit/g:sm/q:40/h:150/w:150/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNy02/MTg5LmpwZWc.jpeg">https://i.discogs.com/fjy0PGAGHsHIXex5HqMDitJI0Yh3MesiPL6ZOyko4bk/rs:fit/g:sm/q:90/h:902/w:600/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNy02/MTg5LmpwZWc.jpeg</thumb>
  <thumb spoof="" cache="" aspect="thumb" preview="https://i.discogs.com/8rHEak0VmPSeBRQ8kTg7xwARlg0-yqJtlLCjUiWd75c/rs:fit/g:sm/q:40/h:150/w:150/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNi0x/NDgzLmpwZWc.jpeg">https://i.discogs.com/k79VVA9du3LW57naLLjXLlWesxbtygfstnwrHp6Ku84/rs:fit/g:sm/q:90/h:450/w:600/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNi0x/NDgzLmpwZWc.jpeg</thumb>
  <thumb spoof="" cache="" aspect="thumb" preview="https://i.discogs.com/KtxVQl-q2BzNmIP0hk-Ip8AKwCYZZP0-excxgmMMi68/rs:fit/g:sm/q:40/h:150/w:150/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNi01/MTY1LmpwZWc.jpeg">https://i.discogs.com/TLjVejJmVWFkQuAhXndIV0Ovt-1GJ5mHE5NWr3MNXGk/rs:fit/g:sm/q:90/h:399/w:600/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNi01/MTY1LmpwZWc.jpeg</thumb>
  <thumb spoof="" cache="" aspect="thumb" preview="https://i.discogs.com/Qf4r5w-aRA9ysSo53rnI0E-xDM8XaB7R4CGiHwla_a8/rs:fit/g:sm/q:40/h:150/w:150/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNy0y/Mjc5LmpwZWc.jpeg">https://i.discogs.com/Y8V1WqvgdSmcIsgC2CAq_VNhfaWTX9gWJi9bQ6c0Vno/rs:fit/g:sm/q:90/h:398/w:600/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNy0y/Mjc5LmpwZWc.jpeg</thumb>
  <thumb spoof="" cache="" aspect="thumb" preview="https://i.discogs.com/8a-6X1gRPL0h4PqUBwCffauybMNwz8JYJ81E6JyjnpY/rs:fit/g:sm/q:40/h:150/w:150/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNy00/MzcyLmpwZWc.jpeg">https://i.discogs.com/OD2sPGIfSZGnrT6JyfKmlO7kuX4ZadJhP-iNaEzbbuE/rs:fit/g:sm/q:90/h:902/w:600/czM6Ly9kaXNjb2dz/LWRhdGFiYXNlLWlt/YWdlcy9BLTMyNDMt/MTQwMjQwMTYxNy00/MzcyLmpwZWc.jpeg</thumb>
  <genre clear="true">Acid House / Hardcore / Techno / Acid / Breakbeat / Minimal / Tech House / Tribal</genre>
  <album>
    <title>The Penguin / Scissorhands</title>
    <year>1996</year>
  </album>
  <album>
    <title>The Minneapolis Sessions (2016 Reissue)</title>
    <year>1997</year>
  </album>
  <album>
    <title>Over The Hill</title>
    <year>2001</year>
  </album>
  <album>
    <title>Over The Hill Remixes</title>
    <year>2001</year>
  </album>
  <album>
    <title>Pleasure Unit</title>
    <year>2016</year>
  </album>
</artist>)" };

        const ArtistInfo artistInfo{ parseArtistInfo(is) };

        EXPECT_EQ(artistInfo.mbid, core::UUID::fromString("38811c52-85e3-4e2e-3319-ab7d9f2cfa5b"));
        EXPECT_EQ(artistInfo.name, "Tim Taylor");
        ASSERT_EQ(artistInfo.sortName, "Taylor, Tim");
        ASSERT_EQ(artistInfo.disambiguation, "Timothy Taylor");
        ASSERT_EQ(artistInfo.biography, "DJ and producer based in London, UK. Founder of Missile Records and Planet Of Drums.\r\n\r\nHe moved from the UK to Montreal in 1984 to become resident DJ at a number of clubs. In 1987, he began working as an A&R for JSE Agency & Management in New York, managing the likes of Tommy Musto, Frankie Bones, and The KLF. He also arranged and was tour manager for artists such as Womack & Womack, Jungle Brothers, Ice-T, and Guru Josh.");
    }

    TEST(ArtistInfo, trim)
    {
        std::istringstream is{ R"(<?xml version='1.0' encoding='UTF-8' standalone='yes'?>
<artist>
  <name> My Artist  </name>
  <musicBrainzArtistID>  38811c52-85e3-4e2e-3319-ab7d9f2cfa5b  </musicBrainzArtistID>
  <sortname>   Artist, My  </sortname>
  <disambiguation>  My Artist   </disambiguation>
</artist>)" };

        const ArtistInfo artistInfo{ parseArtistInfo(is) };

        EXPECT_EQ(artistInfo.mbid, core::UUID::fromString("38811c52-85e3-4e2e-3319-ab7d9f2cfa5b"));
        EXPECT_EQ(artistInfo.name, "My Artist");
        ASSERT_EQ(artistInfo.sortName, "Artist, My");
        ASSERT_EQ(artistInfo.disambiguation, "My Artist");
    }
} // namespace lms::metadata::tests