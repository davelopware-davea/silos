#pragma once

#include <cstddef>
#include <cstdint>

constexpr std::size_t PokeStringCapacity = 49U;
constexpr std::size_t PokeSymbolCapacity = 17U;
constexpr std::size_t PokeListCapacity = 8U;
constexpr std::size_t PokeValueCapacity = 16U;
constexpr std::size_t PokeDepthCapacity = 4U;

enum class SerializedValueKind { Nil, Boolean, Integer, Symbol, String, List };
constexpr std::uint8_t InvalidSerializedValueIndex = PokeValueCapacity;

struct SerializedValue {
  SerializedValueKind kind = SerializedValueKind::Nil;
  int integer = 0;
  char text[PokeStringCapacity]{};
  std::uint8_t children[PokeListCapacity]{};
  std::uint8_t child_count = 0;
};

struct SerializedPayload {
  SerializedValue values[PokeValueCapacity]{};
  std::uint8_t value_count = 0;
  std::uint8_t root = InvalidSerializedValueIndex;
};

enum class ShellEventKind { AppInitialise, Poke };

struct ShellEvent {
  ShellEventKind kind = ShellEventKind::AppInitialise;
  std::size_t app_index = 0;
  std::uint32_t app_generation = 0;
  SerializedPayload payload{};
};

struct ShellRequest {
  std::size_t app_index = 0;
  std::uint32_t app_generation = 0;
  SerializedPayload payload{};
};
