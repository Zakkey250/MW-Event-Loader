#pragma once
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace native_cash {
// GRaceParameters::GetCashValue (005FB920) loads CashValue with FLD.
// Preserve that native float32 contract even though the editor uses integers.
inline std::uint32_t Bits(unsigned amount) {
 const float value=static_cast<float>(amount);
 if(static_cast<double>(value)>std::numeric_limits<int>::max())throw std::runtime_error("Cash reward exceeds native signed payout range");
 std::uint32_t bits=0;static_assert(sizeof(value)==sizeof(bits));
 std::memcpy(&bits,&value,sizeof(bits));return bits;
}
inline float Value(unsigned amount) {
 const auto bits=Bits(amount);float value=0;std::memcpy(&value,&bits,sizeof(value));return value;
}
}
