#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace mwel_event_roads {
using Bytes = std::string_view;
using Point = std::array<float, 3>;

template<class T> T Read(Bytes data, size_t offset) {
 T value{};
 std::memcpy(&value, data.data() + offset, sizeof(value));
 return value;
}

struct Master {
 Bytes road;
 std::vector<std::pair<uint32_t, Bytes>> otherChunks;
 bool Parse(Bytes data) {
  for (size_t at = 0; at < data.size();) {
   if (data.size() - at < 8) return false;
   const auto tag = Read<uint32_t>(data, at), size = Read<uint32_t>(data, at + 4);
   at += 8;
   if (size > data.size() - at) return false;
   auto chunk = data.substr(at, size);
   if (tag == 0x3B800) {
    if (!road.empty() || chunk.empty()) return false;
    road = chunk;
   } else if (tag != 0) otherChunks.emplace_back(tag, chunk);
   at += size;
  }
  return !road.empty();
 }
};

struct Graph {
 Bytes nodes, edges, profiles, classes;
 static constexpr size_t NodeSize = 32, EdgeSize = 22, ProfileSize = 64, ClassSize = 8;
 bool Parse(Bytes road) {
  const auto marker = road.substr(0, 128).find("PRAC");
  if (marker == Bytes::npos) return false;
  std::map<Bytes, Bytes> tables;
  bool ended = false;
  for (size_t at = marker + 16; at + 16 <= road.size() && at < marker + 4096; at += 16) {
   const auto tag = road.substr(at, 4);
   if (tag == "\xaa\xaa\xaa\xaa") { ended = true; break; }
   const auto stride = tag == "dnNR" ? NodeSize : tag == "gsNR" ? EdgeSize :
    tag == "fpNR" ? ProfileSize : tag == "drNR" ? ClassSize : 0;
   if (!stride) continue;
   const auto packed = Read<uint32_t>(road, at + 4), count = Read<uint32_t>(road, at + 8);
   const auto relative = Read<uint32_t>(road, at + 12);
   if ((packed & 255) != 2 || count > 65535 || (packed >> 8) != count * stride ||
       relative < 16 || relative > road.size() - at || count * stride > road.size() - at - relative ||
       !tables.emplace(tag, road.substr(at + relative, count * stride)).second) return false;
  }
  if (!ended || tables.size() != 4) return false;
  nodes = tables["dnNR"]; edges = tables["gsNR"]; profiles = tables["fpNR"]; classes = tables["drNR"];
  return !nodes.empty() && !edges.empty();
 }
 bool HasEdge(size_t id) const { return id < edges.size() / EdgeSize; }
 Bytes Edge(size_t id) const { return edges.substr(id * EdgeSize, EdgeSize); }
 Bytes Node(size_t id) const { return nodes.substr(id * NodeSize, NodeSize); }
 size_t NodeCount() const { return nodes.size() / NodeSize; }
 Point Position(size_t id) const {
  const auto node = Node(id);
  return {Read<float>(node, 8), -Read<float>(node, 0), Read<float>(node, 4)};
 }

 std::string EdgeShape(size_t id) const {
  if (!HasEdge(id)) return {};
  const auto edge = Edge(id);
  if (Read<uint16_t>(edge, 8) != id) return {};
  // Table ordinals and references can move after an insertion or compaction.
  // Compare referenced geometry and driving properties by value instead.
  std::string shape(edge.substr(4, 2));
  shape.append(edge.substr(10));
  const auto roadClass = Read<int16_t>(edge, 6);
  shape += roadClass < 0 ? '\0' : '\1';
  if (roadClass >= 0) {
   if (size_t(roadClass) >= classes.size() / ClassSize) return {};
   shape.append(classes.substr(size_t(roadClass) * ClassSize, ClassSize));
  }
  for (size_t offset : {0, 2}) {
   const auto nodeId = Read<uint16_t>(edge, offset);
   if (nodeId >= NodeCount()) return {};
   const auto node = Node(nodeId);
   const auto degree = Read<uint8_t>(node, 16);
   if (degree > 7) return {};
   bool linked = false;
   for (uint8_t i = 0; i < degree; ++i) linked |= Read<uint16_t>(node, 18 + i * 2) == id;
   if (!linked) return {};
   for (auto component : Position(nodeId)) if (!std::isfinite(component)) return {};
   shape.append(node.substr(0, 12));
   const auto profile = Read<uint16_t>(node, 14);
   shape += profile == 0xffff ? '\0' : '\1';
   if (profile != 0xffff) {
    if (profile >= profiles.size() / ProfileSize) return {};
    shape.append(profiles.substr(size_t(profile) * ProfileSize, ProfileSize));
   }
  }
  return shape;
 }
};

struct Changes {
 std::vector<bool> unchanged;
 struct Segment { Point start, end; };
 std::vector<Segment> changedRoads;

 bool Compare(Bytes before, Bytes after) {
  unchanged.clear();
  changedRoads.clear();
  Master source, current;
  Graph a, b;
  if (!source.Parse(before) || !current.Parse(after) ||
      !a.Parse(source.road) || !b.Parse(current.road)) return false;
  std::map<std::string, size_t> currentRoads;
  for (size_t id = 0; id < b.edges.size() / Graph::EdgeSize; ++id) {
   auto shape = b.EdgeShape(id);
   if (!shape.empty()) currentRoads.emplace(std::move(shape), id);
  }
  unchanged.resize(a.edges.size() / Graph::EdgeSize);
  const auto record = [&](const Graph& graph, size_t id) {
   if (!graph.HasEdge(id)) return true;
   const auto edge = graph.Edge(id);
   const auto start = Read<uint16_t>(edge, 0), end = Read<uint16_t>(edge, 2);
   if (start >= graph.NodeCount() || end >= graph.NodeCount()) return false;
   const auto p = graph.Position(start), q = graph.Position(end);
   for (size_t axis = 0; axis < p.size(); ++axis)
    if (!std::isfinite(p[axis]) || !std::isfinite(q[axis])) return false;
   changedRoads.push_back({p, q});
   return true;
  };
  // Keep source IDs for exported event references. Native AI, progress and
  // wall assets already contain world coordinates, not receiving table IDs.
  for (size_t id = 0; id < unchanged.size(); ++id) {
   const auto shape = a.EdgeShape(id);
   unchanged[id] = !shape.empty() && currentRoads.find(shape) != currentRoads.end();
   if (!unchanged[id] && !record(a, id)) return false;
  }
  return true;
 }

 bool KeepsEdge(size_t id) const { return id < unchanged.size() && unchanged[id]; }

 bool KeepsPosition(const Point& point) const {
  // The two adjacent road lanes and a modest margin cover nearby free placements.
  constexpr float clearance = 12.f;
  for (const auto& road : changedRoads) {
   float lengthSquared = 0, along = 0;
   for (size_t axis = 0; axis < point.size(); ++axis) {
    const auto delta = road.end[axis] - road.start[axis];
    lengthSquared += delta * delta;
    along += (point[axis] - road.start[axis]) * delta;
   }
   const auto t = lengthSquared > 0 ? std::max(0.f, std::min(1.f, along / lengthSquared)) : 0.f;
   float distanceSquared = 0;
   for (size_t axis = 0; axis < point.size(); ++axis) {
    const auto delta = point[axis] - (road.start[axis] + t * (road.end[axis] - road.start[axis]));
    distanceSquared += delta * delta;
   }
   if (distanceSquared < clearance * clearance) return false;
  }
  return true;
 }
};
}
