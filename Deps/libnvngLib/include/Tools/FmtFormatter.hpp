#pragma once

namespace fmt {
        template <typename T, typename M>
        struct formatter<std::pair<T, M>> {
                template <typename ParseContext>
                consteval inline auto parse(ParseContext& ctx) { return ctx.begin(); }

                template <typename FormatContext>
                consteval inline auto format(const std::pair<T, M>& p, FormatContext &ctx) {
                        return format_to(ctx.out(), "{} {}", p.first, p.second);
                }
        };
}