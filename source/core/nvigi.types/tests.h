// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "source/core/nvigi.types/types.h"

//! Unit tests for various custom types we use to replace STL
//! 
namespace nvigi
{

namespace types
{

#ifndef NVIGI_PRODUCTION
TEST_CASE("types::vector supports empty containers", "[types][vector]") {
    nvigi::types::vector<int> vec;
    REQUIRE(vec.empty());
}

TEST_CASE("types::vector supports adding elements", "[types][vector]") {
    nvigi::types::vector<int> vec;
    vec.push_back(1);
    REQUIRE(vec.size() == 1);
    REQUIRE(vec[0] == 1);
}

TEST_CASE("types::vector can be resized", "[types][vector]") {
    nvigi::types::vector<int> vec(5); // Size 5, default-initialized elements (0)
    REQUIRE(vec.size() == 5);
    vec.resize(10);
    REQUIRE(vec.size() == 10);
    REQUIRE(vec[9] == 0); // Check that new elements are default-initialized
}

TEST_CASE("types::vector supports range-based for loops", "[types][vector]") {
    nvigi::types::vector<int> vec = { 1, 2, 3, 4, 5 };
    int sum = 0;
    for (auto& item : vec) {
        sum += item;
    }
    REQUIRE(sum == 15);
}
#ifdef NVIGI_VALIDATE_MEMORY
TEST_CASE("types::vector supports complex type destruction", "[types][vector]") {
    auto prevAllocs = params.imem->getNumAllocations();
    {
        types::vector<types::string> vec;
        vec.push_back("Hello");
        vec.push_back("World");
        vec.push_back("Test");
    } // vec goes out of scope here, and its destructor should be called
    REQUIRE(params.imem->getNumAllocations() == prevAllocs);
}
#endif

TEST_CASE("Random number can be found in the randomly generated vector", "[types][vector]") {
    std::srand(static_cast<unsigned int>(std::time(nullptr))); // Seed random number generator

    // Generate random size for the vector (between 1 and 100)
    int size = std::rand() % 100 + 1;

    types::vector<int> vec;
    vec.reserve(size);

    // Fill the vector with random numbers (for simplicity, between 1 and 100)
    for (int i = 0; i < size; ++i) {
        vec.push_back(std::rand() % 100 + 1);
    }

    // Randomly pick one number from the vector
    int randomIndex = std::rand() % size;
    int pickedNumber = vec[randomIndex];

    // Check if the picked number can be found in the vector
    REQUIRE(vec.contains(pickedNumber));
}

//! STRING

TEST_CASE("std::string supports constructing empty strings", "[types][string]") {
    nvigi::types::string str;
    REQUIRE(str.empty());
}

TEST_CASE("std::string supports appending text", "[types][string]") {
    nvigi::types::string str = "Hello";
    str += ", World!";
    REQUIRE(str == "Hello, World!");
}

TEST_CASE("std::string supports substring operations", "[types][string]") {
    nvigi::types::string str = "Hello, World!";
    auto substr = str.substr(0, 5); // "Hello"
    REQUIRE(substr == "Hello");
}

TEST_CASE("std::string finds substring positions correctly", "[types][string]") {
    nvigi::types::string str = "Hello, World!";
    auto pos = str.find("World");
    REQUIRE(pos != std::string::npos);
    REQUIRE(pos == 7);
}
#endif

}
}