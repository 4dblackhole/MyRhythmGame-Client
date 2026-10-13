#pragma once
#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace finger_drum::chart
{
    // Read-only projection of intervals. Owning editor events remain in their
    // trees; rebuilding this balanced index never changes IDs or source beats.
    template <class Coordinate, class Value> class IntervalIndex final
    {
      public:
        struct Entry
        {
            Coordinate begin{}, end{};
            Value value{};
        };
        void Assign(std::vector<Entry> entries)
        {
            std::ranges::stable_sort(entries, {}, &Entry::begin);
            IntervalIndex replacement;
            replacement.nodes_.reserve(entries.size());
            replacement.root_ = replacement.Build(entries, 0, entries.size());
            *this = std::move(replacement);
        }
        template <class Visitor> void Visit(Coordinate begin, Coordinate end, Visitor &&visitor) const
        {
            if (end < begin)
                return;
            VisitNode(root_, begin, end, visitor);
        }

      private:
        static constexpr std::size_t None = std::numeric_limits<std::size_t>::max();
        struct Node
        {
            Entry entry;
            Coordinate maximumEnd;
            std::size_t left{None}, right{None};
        };
        std::size_t Build(const std::vector<Entry> &entries, std::size_t begin, std::size_t end)
        {
            if (begin == end)
                return None;
            const auto middle = begin + (end - begin) / 2;
            const auto index = nodes_.size();
            nodes_.push_back({entries[middle], entries[middle].end});
            const auto left = Build(entries, begin, middle);
            const auto right = Build(entries, middle + 1, end);
            auto &node = nodes_[index];
            node.left = left;
            node.right = right;
            if (left != None)
                node.maximumEnd = std::max(node.maximumEnd, nodes_[left].maximumEnd);
            if (right != None)
                node.maximumEnd = std::max(node.maximumEnd, nodes_[right].maximumEnd);
            return index;
        }
        template <class Visitor>
        void VisitNode(std::size_t index, Coordinate begin, Coordinate end, Visitor &visitor) const
        {
            if (index == None || nodes_[index].maximumEnd < begin)
                return;
            const auto &node = nodes_[index];
            VisitNode(node.left, begin, end, visitor);
            if (node.entry.begin > end)
                return;
            if (node.entry.end >= begin)
                visitor(node.entry.value);
            VisitNode(node.right, begin, end, visitor);
        }
        std::vector<Node> nodes_;
        std::size_t root_{None};
    };
} // namespace finger_drum::chart
