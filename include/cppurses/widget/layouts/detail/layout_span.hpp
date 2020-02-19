#ifndef CPPURSES_WIDGET_LAYOUTS_DETAIL_LAYOUT_SPAN_HPP
#define CPPURSES_WIDGET_LAYOUTS_DETAIL_LAYOUT_SPAN_HPP
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <utility>
#include <vector>

#include <cppurses/widget/size_policy.hpp>
#include <cppurses/widget/widget.hpp>

namespace cppurses::layout::detail {

struct Dimension {
    Widget* widget;  // This is nullptr when limit is reached
    std::size_t length;
};

/// Container view to iterate over a Widget's children, yielding layout info.
/** Shrinks on each iteration by dropping elements that have reached some limit.
 *  \p Get_policy_t: A functor type <Size_policy const&(Widget const&)>
 *                   Used to retrieve a Size_policy to work with. */
template <typename Get_policy_t>
class Layout_span {
   private:
    using Container_t = std::vector<Dimension>;

    /// \p Get_limit_t Is a functor type <std::size_t(Size_policy const&)>
    template <typename Get_limit_t>
    class Iterator {
       private:
        using Underlying_t = Container_t::iterator;

       public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = Underlying_t::value_type;
        using difference_type   = Underlying_t::difference_type;
        using pointer           = Underlying_t::pointer;
        using reference         = Underlying_t::reference;

       public:
        /// This will increment the position in the container until is valid.
        Iterator(Underlying_t iter,
                 Underlying_t end,
                 Get_policy_t get_policy,
                 Get_limit_t get_limit)
            : iter_{iter},
              end_{end},
              get_policy_{get_policy},
              get_limit_{get_limit}
        {
            while (iter_ != end && iter_->widget == nullptr)
                ++iter_;
        }

        Iterator()                = delete;
        Iterator(Iterator const&) = delete;
        Iterator(Iterator&&)      = delete;
        auto operator=(Iterator const&) -> Iterator& = delete;
        auto operator=(Iterator &&) -> Iterator& = delete;

        auto operator++() -> Iterator&
        {
            if (iter_->length == get_limit_(get_policy_(*iter_->widget)))
                iter_->widget = nullptr;
            do {
                ++iter_;
            } while (iter_ != end_ && iter_->widget == nullptr);
            return *this;
        }

        auto operator*() const -> reference { return *iter_; }

        auto operator-> () const -> pointer { return iter_.operator->(); }

        auto operator==(Container_t::iterator other) const -> bool
        {
            return this->iter_ == other;
        }

        auto operator!=(Container_t::iterator other) const -> bool
        {
            return this->iter_ != other;
        }

        auto get_policy() const -> Size_policy const&
        {
            return get_policy_(*iter_->widget);
        }

       private:
        Underlying_t iter_;
        Underlying_t end_;
        Get_policy_t get_policy_;
        Get_limit_t get_limit_;
    };

    // For Clang
    template <typename Get_limit_t>
    Iterator(typename Layout_span::Container_t::iterator iter,
             typename Layout_span::Container_t::iterator end,
             Get_policy_t get_policy,
             Get_limit_t get_limit)
        ->Iterator<decltype(get_limit)>;

   public:
    template <typename Child_view_t>
    Layout_span(Child_view_t children,
                std::size_t primary_length,
                Get_policy_t&& get_policy)
        : dimensions_{Layout_span::build_dimensions(children,
                                                    primary_length,
                                                    get_policy)},
          get_policy_{std::forward<Get_policy_t>(get_policy)}
    {}

    /// Return iterator to the first element, will skip when length == max
    /** Size_policy::max will be used as limit. */
    auto begin_max()
    {
        total_stretch_ = this->calculate_total_stretch();
        return this->begin([](Size_policy const& p) { return p.max(); });
    }

    /// Return iterator to the first element, will skip when length == min
    /** Size_policy::min will be used as limit. */
    auto begin_min()
    {
        total_inverse_stretch_ = this->calculate_total_inverse_stretch();
        return this->begin([](Size_policy const& p) { return p.min(); });
    }

    auto end() -> Container_t::iterator { return dimensions_.end(); }

    auto total_stretch() const -> double { return total_stretch_; }

    auto total_inverse_stretch() const -> double
    {
        return total_inverse_stretch_;
    }

    auto entire_length() const -> std::size_t
    {
        return std::accumulate(dimensions_.begin(), dimensions_.end(), 0uL,
                               [](std::size_t total, Dimension const& d) {
                                   return total + d.length;
                               });
    }

    auto size() const -> std::size_t
    {
        return std::count_if(dimensions_.begin(), dimensions_.end(),
                             [](auto const& d) { return d.widget != nullptr; });
    }

    auto get_results() const -> std::vector<std::size_t>
    {
        auto result = std::vector<std::size_t>{};
        result.reserve(dimensions_.size());
        std::transform(dimensions_.begin(), dimensions_.end(),
                       std::back_inserter(result),
                       [](auto const& d) { return d.length; });
        return result;
    }

   private:
    Container_t dimensions_;
    Get_policy_t get_policy_;
    mutable double total_stretch_         = 0.;
    mutable double total_inverse_stretch_ = 0.;

   private:
    template <typename Child_view_t>
    static auto build_dimensions(Child_view_t children,
                                 std::size_t primary_length,
                                 Get_policy_t get_policy) -> Container_t
    {
        auto result = Container_t{};
        result.reserve(children.size());
        auto min_running_total = 0uL;
        for (auto& child : children) {
            min_running_total += get_policy(child).min();
            if (min_running_total > primary_length)
                result.push_back({nullptr, 0uL});
            else
                result.push_back({&child, get_policy(child).hint()});
        }
        return result;
    }

    /// \p Get_limit_t Is a functor type <std::size_t(Size_policy const&)>
    /** Calculates and stores total stretch when called. */
    template <typename Get_limit_t>
    auto begin(Get_limit_t get_limit)
    {
        auto const begin = dimensions_.begin();
        auto const end   = dimensions_.end();
        auto temp        = Iterator{begin, end, get_policy_, get_limit};
        while (temp != end)
            ++temp;  // This call invalidates elements that are at limit.
        return Iterator{begin, end, get_policy_, get_limit};
    }

    auto calculate_total_stretch() const -> double
    {
        auto sum       = 0.;
        auto const end = dimensions_.end();
        for (auto iter = dimensions_.begin(); iter != end; ++iter) {
            if (iter->widget != nullptr)
                sum += get_policy_(*iter->widget).stretch();
        }
        return sum;
    }

    auto calculate_total_inverse_stretch() const -> double
    {
        auto sum       = 0.;
        auto const end = dimensions_.end();
        for (auto iter = dimensions_.begin(); iter != end; ++iter) {
            if (iter->widget != nullptr)
                sum += (1. / get_policy_(*iter->widget).stretch());
        }
        return sum;
    }

    auto find_valid_begin() const
    {
        auto const end = dimensions_.end();
        for (auto iter = dimensions_.begin(); iter != end; ++iter) {
            if (iter->widget != nullptr)
                return iter;
        }
        return end;
    }
};

}  // namespace cppurses::layout::detail
#endif  // CPPURSES_WIDGET_LAYOUTS_DETAIL_LAYOUT_SPAN_HPP