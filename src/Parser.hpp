// azul
// Copyright © 2016-2017 Ken Arroyo Ohori
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef Parser_hpp
#define Parser_hpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <list>
#include <vector>
#include <map>
#include <limits>

#include <pugixml-1.8/pugixml.hpp>

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/linear_least_squares_fitting_3.h>

#include "json.hpp"

typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;
typedef CGAL::Exact_predicates_tag Tag;
typedef CGAL::Triangulation_vertex_base_2<Kernel> VertexBase;
typedef CGAL::Constrained_triangulation_face_base_2<Kernel> FaceBase;
typedef CGAL::Triangulation_face_base_with_info_2<std::pair<bool, bool>, Kernel, FaceBase> FaceBaseWithInfo;
typedef CGAL::Triangulation_data_structure_2<VertexBase, FaceBaseWithInfo> TriangulationDataStructure;
typedef CGAL::Constrained_Delaunay_triangulation_2<Kernel, TriangulationDataStructure, Tag> Triangulation;

struct ParsedPoint {
  float coordinates[3];
};

struct ParsedRing {
  std::list<ParsedPoint> points;
};

struct ParsedPolygon {
  ParsedRing exteriorRing;
  std::list<ParsedRing> interiorRings;
};

struct ParsedObject {
  std::string type;
  std::string id;
  std::map<std::string, std::string> attributes;
  std::map<std::string, std::list<ParsedPolygon>> polygonsByType;
  std::map<std::string, std::vector<float>> trianglesByType;
  std::vector<float> edges;
};

struct PointsWalker: pugi::xml_tree_walker {
  std::list<ParsedPoint> points;
  virtual bool for_each(pugi::xml_node &node) {
    if (strcmp(node.name(), "gml:pos") == 0 ||
        strcmp(node.name(), "gml:posList") == 0) {
      //      std::cout << node.name() << " " << node.child_value() << std::endl;
      std::string coordinates(node.child_value());
      std::istringstream iss(coordinates);
      unsigned int currentCoordinate = 0;
      do {
        std::string substring;
        iss >> substring;
        if (substring.length() > 0) {
          if (currentCoordinate == 0) points.push_back(ParsedPoint());
          try {
            points.back().coordinates[currentCoordinate] = std::stof(substring);
          } catch (const std::invalid_argument& ia) {
            std::cout << "Invalid point: " << substring << ". Skipping..." << std::endl;
            points.clear();
            return true;
          } currentCoordinate = (currentCoordinate+1)%3;
        }
      } while (iss);
      if (currentCoordinate != 0) {
        std::cout << "Wrong number of coordinates: not divisible by 3" << std::endl;
        points.clear();
      } //std::cout << "Created " << points.size() << " points" << std::endl;
    } return true;
  }
};

struct RingsWalker: pugi::xml_tree_walker {
  pugi::xml_node exteriorRing;
  std::list<pugi::xml_node> interiorRings;
  virtual bool for_each(pugi::xml_node &node) {
    if (strcmp(node.name(), "gml:exterior") == 0) {
      exteriorRing = node;
    } else if (strcmp(node.name(), "gml:interior") == 0) {
      interiorRings.push_back(node);
    } return true;
  }
};

struct PolygonsWalker: pugi::xml_tree_walker {
  std::map<std::string, std::list<pugi::xml_node>> polygonsByType;
  std::string inDefinedType = "";  // "" = undefined
  unsigned int depthToStop;
  virtual bool for_each(pugi::xml_node &node) {
    const char *nodeType = node.name();
    const char *namespaceSeparator = strchr(nodeType, ':');
    if (namespaceSeparator != NULL) {
      nodeType = namespaceSeparator+1;
    }
    
    if (inDefinedType != "" && depth() <= depthToStop) {
      inDefinedType = "";
    } if (strcmp(nodeType, "Door") == 0 ||
          strcmp(nodeType, "GroundSurface") == 0 ||
          strcmp(nodeType, "RoofSurface") == 0 ||
          strcmp(nodeType, "Window") == 0) {
      inDefinedType = nodeType;
      depthToStop = depth();
    } else if (strcmp(nodeType, "Polygon") == 0 ||
               strcmp(nodeType, "Triangle") == 0) {
      polygonsByType[inDefinedType].push_back(node);
    } return true;
  }
};

struct ObjectsWalker: pugi::xml_tree_walker {
  std::list<pugi::xml_node> objects;
  virtual bool for_each(pugi::xml_node &node) {
    const char *nodeType = node.name();
    const char *namespaceSeparator = strchr(nodeType, ':');
    if (namespaceSeparator != NULL) {
      nodeType = namespaceSeparator+1;
    }
    
    if (strcmp(nodeType, "AuxiliaryTrafficArea") == 0 ||
        strcmp(nodeType, "Bridge") == 0 ||
        strcmp(nodeType, "Building") == 0 ||
        strcmp(nodeType, "BuildingPart") == 0 ||
        strcmp(nodeType, "BuildingInstallation") == 0 ||
        strcmp(nodeType, "CityFurniture") == 0 ||
        strcmp(nodeType, "GenericCityObject") == 0 ||
        strcmp(nodeType, "LandUse") == 0 ||
        strcmp(nodeType, "PlantCover") == 0 ||
        strcmp(nodeType, "Railway") == 0 ||
        strcmp(nodeType, "ReliefFeature") == 0 ||
        strcmp(nodeType, "Road") == 0 ||
        strcmp(nodeType, "SolitaryVegetationObject") == 0 ||
        strcmp(nodeType, "TrafficArea") == 0 ||
        strcmp(nodeType, "Tunnel") == 0 ||
        strcmp(nodeType, "WaterBody") == 0) {
      objects.push_back(node);
    } return true;
  }
};

class Parser {
public:
  std::list<ParsedObject> objects;
  
  bool firstRing;
  float minCoordinates[3];
  float maxCoordinates[3];
  
  std::set<std::string> attributesToPreserve;
  
  std::list<ParsedObject>::const_iterator currentObject;
  std::map<std::string, std::vector<float>>::const_iterator currentTrianglesBuffer;
  std::map<std::string, std::string>::const_iterator currentAttribute;
  
  Parser();
  void parseCityGML(const char *filePath);
  void parseCityJSON(const char *filePath);
  void clear();
  
  void parseCityGMLObject(pugi::xml_node &node, ParsedObject &object);
  void parseCityGMLPolygon(pugi::xml_node &node, ParsedPolygon &polygon);
  void parseCityGMLRing(pugi::xml_node &node, ParsedRing &ring);
  
  void parseCityJSONObject(nlohmann::json::const_iterator &iterator, ParsedObject &object, std::vector<std::vector<double>> &vertices);
  void parseCityJSONPolygon(const std::vector<std::vector<std::size_t>> &jsonPolygon, ParsedPolygon &polygon, std::vector<std::vector<double>> &vertices);
  void parseCityJSONRing(const std::vector<std::size_t> &jsonRing, ParsedRing &ring, std::vector<std::vector<double>> &vertices);
  
  void centroidOf(ParsedRing &ring, ParsedPoint &centroid);
  void addTrianglesFromTheConstrainedTriangulationOfPolygon(ParsedPolygon &polygon, std::vector<float> &triangles);
  void regenerateTrianglesFor(ParsedObject &object);
  void regenerateEdgesFor(ParsedObject &object);
  void regenerateGeometries();
};

#endif /* Parser_hpp */