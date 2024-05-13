/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018-2023 Roman Lebedev

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

#include "adt/Invariant.h"
#include "codes/AbstractPrefixCode.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <initializer_list> // IWYU pragma: keep
#include <memory>

namespace rawspeed {

template <typename CodeTag>
class BinaryPrefixTree final /* : public BinarySearchTree */ {
public:
  using Traits = typename AbstractPrefixCode<CodeTag>::Traits;
  using CodeSymbol = typename AbstractPrefixCode<CodeTag>::CodeSymbol;
  using CodeTy = typename Traits::CodeTy;

  struct Branch;
  struct Leaf;

  struct Node {
    enum class Type : uint8_t { Branch, Leaf };

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
    explicit operator typename Node::Type() const final {
      return Node::Type::Branch;
    }

    std::array<std::unique_ptr<Node>, 2> buds;
  };

  struct Leaf final : public Node {
    explicit operator typename Node::Type() const final {
      return Node::Type::Leaf;
    }

    CodeTy value;

    Leaf() = default;

    explicit Leaf(CodeTy value_) : value(value_) {}
  };

  void add(CodeSymbol symbol, CodeTy value);

  std::unique_ptr<Node> root;
};

template <typename CodeTag>
void BinaryPrefixTree<CodeTag>::add(const CodeSymbol symbol, CodeTy value) {
  invariant(symbol.code_len > 0);
  invariant(symbol.code_len <= Traits::MaxCodeLenghtBits);

  CodeSymbol partial;
  partial.code = 0;
  partial.code_len = 0;

  std::reference_wrapper<std::unique_ptr<Node>> newBud = root;
  for (unsigned bit : symbol.getBitsMSB()) {
    ++partial.code_len;
    partial.code =
        implicit_cast<typename Traits::CodeTy>((partial.code << 1) | bit);
    std::unique_ptr<Node>& bud = newBud;
    if (!bud)
      bud = std::make_unique<Branch>();
    // NOTE: if this bud is already a Leaf, this is not a prefix code.
    newBud = bud->getAsBranch().buds[bit];
  }
  invariant(partial == symbol && "Failed to interpret symbol as bit sequence.");

  std::unique_ptr<Node>& bud = newBud;
  assert(!bud && "This Node should be vacant!");

  // And add this node/leaf to tree in the given position
  bud = std::make_unique<Leaf>(value);
}

} // namespace rawspeed
