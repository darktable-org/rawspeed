/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#pragma once

#include <cassert>          // for assert
#include <initializer_list> // IWYU pragma: keep
#include <memory>           // for unique_ptr
#include <vector>           // for vector

namespace rawspeed {

template <typename T>
class BinaryHuffmanTree final /* : public BinarySearchTree */ {
public:
  struct Branch;
  struct Leaf;

  struct Node {
    enum class Type { Branch, Leaf };

    explicit virtual operator Type() const = 0;

    Branch& getAsBranch() {
      assert(Node::Type::Branch == static_cast<Node::Type>(*this));
      return static_cast<Branch&>(*this);
    }

    Leaf& getAsLeaf() {
      assert(Node::Type::Leaf == static_cast<Node::Type>(*this));
      return static_cast<Leaf&>(*this);
    }

    virtual ~Node() = default;
  };

  struct Branch final : public Node {
    explicit operator typename Node::Type() const override {
      return Node::Type::Branch;
    }

    std::unique_ptr<Node> zero;
    std::unique_ptr<Node> one;

    template <typename Lambda> bool forEachNode(Lambda l) const;
    template <typename Lambda> bool forEachNode(Lambda l);

    bool hasLeafs() const;

    static bool pruneLeaflessBranches(std::unique_ptr<Node>* n);
  };

  struct Leaf final : public Node {
    explicit operator typename Node::Type() const override {
      return Node::Type::Leaf;
    }

    T value;

    Leaf() = default;

    explicit Leaf(T value_) : value(value_) {}
  };

  std::unique_ptr<Node> root;

  std::vector<Branch*> getAllBranchesOfDepth(int depth);
  std::vector<std::unique_ptr<Node>*> getAllVacantNodesAtDepth(int depth);
  void pruneLeaflessBranches();
};

template <typename T>
template <typename Lambda>
bool BinaryHuffmanTree<T>::Branch::forEachNode(Lambda l) const {
  bool done = false;
  // NOTE: The order *IS* important! Left to right, zero to one!
  for (const auto* node : {&zero, &one}) {
    done = l(node);
    if (done)
      return done;
  }
  return done;
}

template <typename T>
template <typename Lambda>
bool BinaryHuffmanTree<T>::Branch::forEachNode(Lambda l) {
  bool done = false;
  // NOTE: The order *IS* important! Left to right, zero to one!
  for (auto* node : {&zero, &one}) {
    done = l(node);
    if (done)
      return done;
  }
  return done;
}

template <typename T> bool BinaryHuffmanTree<T>::Branch::hasLeafs() const {
  return forEachNode([](const std::unique_ptr<Node>* n) {
    assert(n);
    if (!(*n)) // If the node is empty, then it certainly does not have leafs
      return false;
    return Node::Type::Leaf == static_cast<typename Node::Type>(**n);
  });
}

template <typename T>
bool BinaryHuffmanTree<T>::Branch::pruneLeaflessBranches(
    std::unique_ptr<Node>* top) {
  if (!top)
    return false;

  bool foundLeafs = false; // Any leafs in this branch?
  (*top)->getAsBranch().forEachNode([&foundLeafs](std::unique_ptr<Node>* n) {
    assert(n);
    if (!(*n))
      return false; // Nothing to do here, node is empty already, keep going.
    switch (static_cast<typename Node::Type>(**n)) {
    case Node::Type::Branch:
      // Recurse. Any leafs in this branch?
      if (Branch::pruneLeaflessBranches(n))
        foundLeafs = true;
      else
        n->reset(); // Aha, dead branch, prune it!
      break;
    case Node::Type::Leaf:
      foundLeafs = true; // Ok, this is a Leaf, great.
      break;
    }
    return false; // keep going.
  });

  if (!foundLeafs)
    top->reset();

  return foundLeafs;
}

template <typename T>
std::vector<typename BinaryHuffmanTree<T>::Branch*>
BinaryHuffmanTree<T>::getAllBranchesOfDepth(int depth) {
  assert(depth >= 0);

  if (0 == depth) {
    // The root (depth == 0) is is special, and is *always* a Branch.
    if (!root)
      root = std::make_unique<Branch>();
    return {&root->getAsBranch()};
  }

  // Recursively get all branches of previous depth
  auto prevBranches = getAllBranchesOfDepth(depth - 1);

  // Early return in case of no branches on previous depth
  if (prevBranches.empty())
    return {};

  // We will have at most twice as much branches as at the previous depth.
  decltype(prevBranches) branches;
  branches.reserve(2U * prevBranches.size());

  for (const auto& prevBranch : prevBranches) {
    assert(prevBranch);

    prevBranch->forEachNode([&branches](std::unique_ptr<Node>* n) {
      assert(n);
      // If the Node is vacant, make it a branch.
      // The user was supposed to create all the required Leafs before.
      // We shall prune Leaf-less branches at the end
      if (!(*n))
        *n = std::make_unique<Branch>();
      // If this is a branch, add it to the list.
      if (Node::Type::Branch == static_cast<typename Node::Type>(**n))
        branches.emplace_back(&((*n)->getAsBranch()));
      return false; // keep going;
    });
  }
  assert(branches.size() <= 2U * prevBranches.size());

  return branches;
}

template <typename T>
std::vector<std::unique_ptr<typename BinaryHuffmanTree<T>::Node>*>
BinaryHuffmanTree<T>::getAllVacantNodesAtDepth(int depth) {
  assert(depth > 0);

  // Get all branches of previous depth
  auto prevBranches = getAllBranchesOfDepth(depth - 1);

  // Early return in case of no branches on previous depth
  if (prevBranches.empty())
    return {};

  // We will have at most two nodes per each branch on the previous depth.
  std::vector<std::unique_ptr<BinaryHuffmanTree<T>::Node>*> nodes;
  nodes.reserve(2U * prevBranches.size());

  for (const auto& prevBranch : prevBranches) {
    assert(prevBranch);

    auto& b = prevBranch->getAsBranch();

    b.forEachNode([&nodes](std::unique_ptr<Node>* n) {
      assert(n);
      if (!(*n)) // If there is no node already, then record it.
        nodes.emplace_back(n);
      return false; // keep going;
    });
  }
  assert(nodes.size() <= 2U * prevBranches.size());

  return nodes;
}

template <typename T> void BinaryHuffmanTree<T>::pruneLeaflessBranches() {
  Branch::pruneLeaflessBranches(&root);
}

} // namespace rawspeed
