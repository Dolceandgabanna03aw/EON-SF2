#pragma once

#include <concepts>
#include <cstddef>
#include <tuple>

namespace x10::dsp
{

/**
    A memoryless nonlinear transfer curve.

    Implementations supply only the curve and its antiderivative. They must NOT
    implement any antialiasing logic: the ill-conditioning fallback lives in
    exactly one place (Adaa1), so that adding a curve cannot reintroduce the
    0/0 bug. See plan D2.

    @c breakpoints lists the abscissae where f or its derivative is piecewise
    defined, over the test range [-8, 8]. Smooth curves expose an empty array.
    test_curves.cpp uses this to assert that F1 is continuous across every join
    — a discontinuity there produces an output spike, not a subtle tuning error.
*/
template <class C>
concept Curve = requires (float x) {
    { C::f (x) } noexcept -> std::same_as<float>;
    { C::F1 (x) } noexcept -> std::same_as<float>;
    { C::breakpoints.size() } -> std::convertible_to<std::size_t>;
};

/** Anything holding state that must be cleared before a voice is reused. */
template <class T>
concept Resettable = requires (T t) {
    { t.reset() } noexcept;
};

/** A single-sample audio processor. */
template <class P>
concept Processor = requires (P p, float x) {
    { p.process (x) } noexcept -> std::same_as<float>;
    { p.reset() } noexcept;
};

/**
    Resets every member of a state tuple.

    Compound stages hold their state as a std::tuple and clear it through this
    helper rather than through a hand-maintained list of assignments. Adding a
    member then cannot be forgotten at reset time — which is the failure that
    puts an impulse on the first sample after a voice steal. See plan D4.
*/
template <class... Ts>
    requires (Resettable<Ts> && ...)
constexpr void resetAll (std::tuple<Ts...>& state) noexcept
{
    std::apply ([] (auto&... member) noexcept { (member.reset(), ...); }, state);
}

} // namespace x10::dsp
