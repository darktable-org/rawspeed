/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; withexpected even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "decompressors/BinaryHuffmanTree.h" // for BinaryHuffmanTree
#include <gtest/gtest.h> // for AssertionResult, DeathTest, Test, AssertHe...

using rawspeed::BinaryHuffmanTree;

namespace rawspeed_test {

TEST(BinaryHuffmanTreeTest, EmptyByDefault) {
  {
    const BinaryHuffmanTree<int> b;
    ASSERT_FALSE(b.root);
  }
  {
    BinaryHuffmanTree<int> b;
    ASSERT_FALSE(b.root);
  }
  {
    struct T {
      int i;
    };
    const BinaryHuffmanTree<T> b;
    ASSERT_FALSE(b.root);
  }
}

#ifndef NDEBUG
TEST(BinaryHuffmanTreeDeathTest, getAllBranchesOfNegativeDepth) {
  ASSERT_DEATH(
      {
        BinaryHuffmanTree<int> b;
        b.getAllBranchesOfDepth(-1);
        exit(0);
      },
      "depth >= 0");
}
#endif

TEST(BinaryHuffmanTreeTest, getAllBranchesOfDepth_0_Base) {
  BinaryHuffmanTree<int> b;
  const auto zero = b.getAllBranchesOfDepth(0);
  ASSERT_EQ(zero.size(), 1);
  ASSERT_EQ(static_cast<typename decltype(b)::Node::Type>(*(b.root)),
            decltype(b)::Node::Type::Branch);
  ASSERT_FALSE(b.root->getAsBranch().hasLeafs());
  for (const auto& branch : zero) {
    ASSERT_EQ(static_cast<typename decltype(b)::Node::Type>(*branch),
              decltype(b)::Node::Type::Branch);
    ASSERT_FALSE(branch->hasLeafs());
  }
  ASSERT_EQ(zero[0], b.root.get());
}
TEST(BinaryHuffmanTreeTest, getAllBranchesOfDepth_1_Base) {
  BinaryHuffmanTree<int> b;
  const auto one = b.getAllBranchesOfDepth(1);
  ASSERT_EQ(one.size(), 2);
  ASSERT_EQ(static_cast<typename decltype(b)::Node::Type>(*(b.root)),
            decltype(b)::Node::Type::Branch);
  ASSERT_FALSE(b.root->getAsBranch().hasLeafs());
  for (const auto& branch : one) {
    ASSERT_EQ(static_cast<typename decltype(b)::Node::Type>(*branch),
              decltype(b)::Node::Type::Branch);
    ASSERT_FALSE(branch->hasLeafs());
  }
  ASSERT_EQ(one[0], &(b.root->getAsBranch().zero->getAsBranch()));
  ASSERT_EQ(one[1], &(b.root->getAsBranch().one->getAsBranch()));
}

#ifndef NDEBUG
TEST(BinaryHuffmanTreeDeathTest, getAllNodesAtZeroDepth) {
  ASSERT_DEATH(
      {
        BinaryHuffmanTree<int> b;
        b.getAllVacantNodesAtDepth(0);
        exit(0);
      },
      "depth > 0");
}
#endif

TEST(BinaryHuffmanTreeTest, getAllVacantNodesAtDepth_1_Base) {
  BinaryHuffmanTree<int> b;
  const auto one = b.getAllVacantNodesAtDepth(1);
  ASSERT_EQ(one.size(), 2);
  ASSERT_EQ(one[0], &(b.root->getAsBranch().zero));
  ASSERT_EQ(one[1], &(b.root->getAsBranch().one));
}

TEST(BinaryHuffmanTreeTest,
     getAllVacantNodesAtDepth_2_fills_depth_1_with_branches) {
  BinaryHuffmanTree<int> b;
  {
    const auto one = b.getAllVacantNodesAtDepth(1);
    ASSERT_EQ(one.size(), 2);
  }
  const auto two = b.getAllVacantNodesAtDepth(2);
  ASSERT_EQ(two.size(), 4);
  {
    // All vacant nodes on previous depths are auto-filled with Branches
    const auto one = b.getAllVacantNodesAtDepth(1);
    ASSERT_EQ(one.size(), 0);
  }
}

TEST(BinaryHuffmanTreeTest, getAllVacantNodesAtDepth_2_Base) {
  BinaryHuffmanTree<int> b;
  const auto two = b.getAllVacantNodesAtDepth(2);
  ASSERT_EQ(two.size(), 4);
  ASSERT_EQ(two[0], &(b.root->getAsBranch().zero->getAsBranch().zero));
  ASSERT_EQ(two[1], &(b.root->getAsBranch().zero->getAsBranch().one));
  ASSERT_EQ(two[2], &(b.root->getAsBranch().one->getAsBranch().zero));
  ASSERT_EQ(two[3], &(b.root->getAsBranch().one->getAsBranch().one));
}

TEST(BinaryHuffmanTreeTest, pruneLeaflessBranches_purges_all) {
  BinaryHuffmanTree<int> b;
  b.getAllVacantNodesAtDepth(2);
  ASSERT_TRUE(b.root);
  b.pruneLeaflessBranches();
  ASSERT_FALSE(b.root);
}

TEST(BinaryHuffmanTreeTest,
     getAllVacantNodesAtDepth_1_after_adding_1_depth_1_leaf) {
  BinaryHuffmanTree<int> b;
  {
    const auto one = b.getAllVacantNodesAtDepth(1);
    ASSERT_EQ(one.size(), 2);
    ASSERT_FALSE(b.root->getAsBranch().hasLeafs());
    // Add one leaf at the depth of one
    *one.front() = std::make_unique<decltype(b)::Leaf>();
    ASSERT_TRUE(b.root->getAsBranch().hasLeafs());

    // Now let's try pruning
    b.pruneLeaflessBranches();
    ASSERT_TRUE(b.root);
    ASSERT_TRUE(b.root->getAsBranch().hasLeafs());
  }
  {
    const auto one = b.getAllVacantNodesAtDepth(1);
    ASSERT_EQ(one.size(), 1);
    ASSERT_EQ(one[0], &(b.root->getAsBranch().one));
  }
}

TEST(BinaryHuffmanTreeTest,
     getAllVacantNodesAtDepth_2_after_adding_1_depth_1_leaf) {
  BinaryHuffmanTree<int> b;
  {
    const auto two = b.getAllVacantNodesAtDepth(2);
    ASSERT_EQ(two.size(), 4);
    ASSERT_TRUE(b.root);
    ASSERT_FALSE(b.root->getAsBranch().hasLeafs());
    ASSERT_TRUE(b.root->getAsBranch().zero);
    ASSERT_TRUE(b.root->getAsBranch().one);
    ASSERT_FALSE(b.root->getAsBranch().zero->getAsBranch().hasLeafs());
    ASSERT_FALSE(b.root->getAsBranch().one->getAsBranch().hasLeafs());

    // Add one leaf at the depth of two
    *two.front() = std::make_unique<decltype(b)::Leaf>();

    ASSERT_TRUE(b.root);
    ASSERT_FALSE(b.root->getAsBranch().hasLeafs());
    ASSERT_TRUE(b.root->getAsBranch().zero);
    ASSERT_TRUE(b.root->getAsBranch().one);
    ASSERT_TRUE(b.root->getAsBranch().zero->getAsBranch().hasLeafs());
    ASSERT_FALSE(b.root->getAsBranch().one->getAsBranch().hasLeafs());
  }
  {
    const auto two = b.getAllVacantNodesAtDepth(2);
    ASSERT_EQ(two.size(), 3);
    ASSERT_EQ(two[0], &(b.root->getAsBranch().zero->getAsBranch().one));
    ASSERT_EQ(two[1], &(b.root->getAsBranch().one->getAsBranch().zero));
    ASSERT_EQ(two[2], &(b.root->getAsBranch().one->getAsBranch().one));
  }
  {
    // And prune
    b.pruneLeaflessBranches();
    ASSERT_TRUE(b.root);
    ASSERT_FALSE(b.root->getAsBranch().hasLeafs());
    ASSERT_TRUE(b.root->getAsBranch().zero);
    ASSERT_FALSE(b.root->getAsBranch().one);
    ASSERT_TRUE(b.root->getAsBranch().zero->getAsBranch().hasLeafs());
  }
}

TEST(BinaryHuffmanTreeTest,
     getAllVacantNodesAtDepth_2_after_adding_1_depth_1_and_1_depth_2_leaf) {
  BinaryHuffmanTree<int> b;
  {
    const auto one = b.getAllVacantNodesAtDepth(1);
    ASSERT_EQ(one.size(), 2);
    // Add one leaf at the depth of one
    *one.front() = std::make_unique<decltype(b)::Leaf>();
  }
  {
    const auto two = b.getAllVacantNodesAtDepth(2);
    ASSERT_EQ(two.size(), 2);
    // Add one leaf at the depth of two
    *two.front() = std::make_unique<decltype(b)::Leaf>();
  }
  {
    const auto two = b.getAllVacantNodesAtDepth(2);
    ASSERT_EQ(two.size(), 1);
    ASSERT_EQ(two[0], &(b.root->getAsBranch().one->getAsBranch().one));
  }
  {
    // And prune
    b.pruneLeaflessBranches();
    ASSERT_TRUE(b.root);
    ASSERT_TRUE(b.root->getAsBranch().hasLeafs());
    ASSERT_TRUE(b.root->getAsBranch().zero);
    ASSERT_TRUE(b.root->getAsBranch().one);
    ASSERT_TRUE(b.root->getAsBranch().one->getAsBranch().hasLeafs());
    ASSERT_TRUE(b.root->getAsBranch().one->getAsBranch().zero);
    ASSERT_FALSE(b.root->getAsBranch().one->getAsBranch().one);
  }
}

} // namespace rawspeed_test
