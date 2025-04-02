// Copyright 2024 Tencent Inc. All rights reserved.
//

#include <stdio.h>
#include <chrono>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include "common/coord_trans.h"
#include "common/log.h"
#include "mapengine/hadmap_codes.h"
#include "mapengine/hadmap_engine.h"
#include "routingmap/routing_map.h"
#include "structs/hadmap_curve.h"
#include "structs/hadmap_header.h"
#include "structs/hadmap_lane.h"
#include "structs/hadmap_predef.h"
#include "types/map_defs.h"

using namespace hadmap;
using namespace std;
int main(int argn, char** argv) {
  std::string fname = argv[1];
  hadmap::txMapHandle* pHandle = nullptr;
  hadmap::MAP_DATA_TYPE mdp = hadmap::SQLITE;
  if (fname.back() == 'r' || fname.back() == 'R') {
    mdp = hadmap::OPENDRIVE;
  }
  std::cout << fname << std::endl;

  if (hadmap::hadmapConnect(fname.c_str(), mdp, &pHandle) != TX_HADMAP_HANDLE_OK) {
    std::cout << "faild.\n";
    return -1;
  }

  // txRoads _roads;
  // getRoads(pHandle, true, _roads);
  // std::cout << " road " << _roads.size() << std::endl;

  // for (auto it : _roads) {
  //   hadmap::txLaneLinks links;
  //   getLaneLinks(pHandle, it->getId(), ROAD_PKID_INVALID, links);
  //   std::cout << " road id " << it->getId() <<  " links size " << links.size() << std::endl;
  //   for (auto link : links) {
  //     std::cout << " link id " << link->getId() << " from " << link->fromRoadId() << " to " << link->toRoadId()
  //               << std::endl;
  //   }
  // }

  if (hadmap::hadmapOutput("/saturnv/SH1_Small.xodr", hadmap::MAP_DATA_TYPE::OPENDRIVE, pHandle)) {
    std::cout << "faild to save \n";
    return -1;
  }

  // std::cout << "saved" << std::endl;

  return 0;

  // auto routing_map = new hadmap::RoutingMap(hadmap::CoordType::COORD_WGS84, fname);

  // hadmap::txRoute route;
  // hadmap::PointVec start_end;
  // bool dump_data = true;

  // //         <InputPath points="121.174779895956,31.288912223367,0.000;121.175224290000,31.289070550000,-0.000" />
  // double start_lon = 121.174779895956;
  // double start_lat = 31.288912223367;
  // double end_lon = 121.175224290000;
  // double end_lat = 31.289070550000;

  // // // Eigen::Vector2d min;
  // // // Eigen::Vector2d max;
  // // // min.x() = -1.03244e+06;
  // // // min.y() = -1.05542e+06;
  // // // max.x() = 833448;
  // // // max.y() = 1.14189e+06;
  // // // auto center = min-max;
  // // // std::cout << " center x " << center.x() << " center y " << center.y() << std::endl;

  // start_end.push_back(hadmap::txPoint(start_lon, start_lat, 0.053));
  // start_end.push_back(hadmap::txPoint(end_lon, end_lat, 0.099));
  // if (routing_map->routingSync(start_end, route)) {
  //   std::cout << "route size " << route.size() << std::endl;
  //   for (auto& route_node : route)
  //     std::cout << " route id " << route_node.getId() << " node type " << (int)route_node.getRouteType() << " pre id
  //     "
  //               << route_node.getPreId() << " next id " << route_node.getNextId() << " length "
  //               << route_node.getLength() << std::endl;
  // } else {
  //   std::cout << "routing failed " << std::endl;
  // }

  // auto interface = routing_map->getMapInterface();
  // auto lanes = interface->getLanes(hadmap::txPoint(0, 0, 0.0), 100.0);
  // std::cout << " routing lane size " << lanes.size() << std::endl;

  // auto start_time = std::chrono::system_clock::now();
  // if (hadmap::hadmapConnect(fname.c_str(), mdp, &pHandle) != TX_HADMAP_HANDLE_OK) {
  //   std::cout << "faild.\n";
  // }
  // auto end_time = std::chrono::system_clock::now();
  // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
  // std::cout << "duration = "
  //           << static_cast<double>(duration.count()) * std::chrono::microseconds::period::num /
  //                  std::chrono::microseconds::period::den
  //           << " s.\n";

  // txRoads _roads;
  // getRoads(pHandle, true, _roads);

  // // hadmap::txLanes ax_lanes;
  // // txPoint point(121.174780,31.288912,0.000000);
  // // hadmap::getLanes(pHandle, point, 10.0, ax_lanes);
  // // std::cout << "lane size " << ax_lanes.size() << std::endl;

  // // txOdHeaderPtr header_ptr;
  // // getHeader(pHandle, header_ptr);
  // // if (header_ptr) {
  // //   std::cout << "header name " << header_ptr->getName() << std::endl;
  // // } else {
  // //   std::cout << "header is null" << std::endl;
  // // }

  // // std::vector<hadmap::txLaneId> lanids;
  // // std::vector<hadmap::OBJECT_TYPE> objtypes;
  // // hadmap::txObjects objs;
  // // auto nRet = hadmap::getObjects(pHandle, lanids, objtypes, objs);
  // // for (auto& obj : objs) {
  // //   std::cout << " obj id " << obj->getId() << " type " << (int)obj->getObjectType() << std::endl;
  // // }

  // // hadmap::txJunctions junctions;
  // // hadmap::getJunctions(pHandle, junctions);
  // // for (auto& junction : junctions) {
  // //   std::cout << "junction id " << junction->getId() << std::endl;
  // // }

  // hadmap::txLaneLinks links;
  // hadmap::PointVec envelope;
  // envelope.push_back(hadmap::txPoint(start_lon - 10, start_lat - 10, 0.0));
  // envelope.push_back(hadmap::txPoint(start_lon + 10, start_lat + 10, 0.0));

  // getLaneLinks(pHandle, envelope, links);
  // std::cout << "links size " << links.size() << std::endl;
  // for (auto& link : links) {
  //   if (link->fromRoadId() == 5668 || link->toRoadId() == 3177) {
  //     std::cout << "link id " << link->getId() << " unique id " << link->getUniqueId() << std::endl;
  //   }
  //   // std::cout << " link id " << link->getId() << " from " << link->fromRoadId() << " to " << link->toRoadId()
  //   //           << std::endl;
  // }

  // std::cout << " road size " << _roads.size() << std::endl;

  // // txRoadPtr tpr;
  // // auto road = getRoad(pHandle, 2883, true, tpr);
  // // if (tpr) {
  // //   std::cout << "road length " << tpr->getLength() << std::endl;
  // // } else {
  // //   std::cout << "road not found " << std::endl;
  // // }

  // // auto ref_point = getRefPoint(pHandle);
  // // std::cout << "ref point = " << ref_point.x << "," << ref_point.y << "," << ref_point.z << std::endl;

  // // txOdHeaderPtr header;
  // // if (0 == getHeader(pHandle, header)) {
  // //   std::cout << "geo ref = " << header->getGeoReference() << std::endl;
  // // }
  // std::ofstream lanegps(string(argv[1]) + ".lane.gps.txt");
  // std::ofstream llinegps(string(argv[1]) + ".lline.gps.txt");
  // lanegps.setf(std::ios::fixed, std::ios::floatfield);
  // lanegps.precision(12);
  // llinegps.setf(std::ios::fixed, std::ios::floatfield);
  // llinegps.precision(12);
  // // std::cout << "road num = " << _roads.size() << std::endl;

  // // // lon 121.157880 lat 31.303275
  // // txPoint loc;
  // // loc.x = 121.15764748160849;
  // // loc.y = 31.30327514863072;

  // // //,
  // // txLanePtr lane_ptr;

  // // getLane(pHandle, loc, lane_ptr, 1.0);
  // // double s, l;
  // // lane_ptr->xy2sl(loc.x, loc.y, s, l);
  // // std::cout << "road id " << lane_ptr->getRoadId() << " lane arrow " << (int)lane_ptr->getLaneArrow() << " lane
  // // length "
  // //           << lane_ptr->getLength() << " s " << s << " l " << l << std::endl;
  // // txLanes prevLanes;
  // // txLanes nextLanes;
  // // txLaneLinks prevLinks;
  // // txLaneLinks nextLinks;
  // // getPrevLanes(pHandle, lane_ptr, prevLanes);
  // // getNextLanes(pHandle, lane_ptr, nextLanes);
  // // getPrevLaneLinks(pHandle, lane_ptr->getTxLaneId(), prevLinks);
  // // getNextLaneLinks(pHandle, lane_ptr->getTxLaneId(), nextLinks);

  // // for (auto& l : prevLanes) {
  // //   std::cout << "prev lane [" << l->getTxLaneId().roadId << "," << l->getTxLaneId().sectionId << ","
  // //             << l->getTxLaneId().laneId << "] " << std::endl;
  // // }
  // // for (auto& l : nextLanes) {
  // //   std::cout << "next lane [" << l->getTxLaneId().roadId << "," << l->getTxLaneId().sectionId << ","
  // //             << l->getTxLaneId().laneId << "] " << std::endl;
  // // }
  // // for (auto& l : prevLinks) {
  // //   std::cout << "prev link " << l->getId() << std::endl;
  // // }
  // // for (auto& l : nextLinks) {
  // //   std::cout << "next link " << l->getId() << " junction id " << l->getJunctionId() << std::endl;
  // // }
  // if (dump_data) {
  //   for (auto& road_ptr : _roads) {
  //     const txSections& sections = road_ptr->getSections();
  //     // std::cout << "road " << road_ptr->getId() << " from id  " << road_ptr->getPrev() << " to id "
  //     //        << (int)road_ptr->getDirection() << std::endl;
  //     for (auto& section_ptr : sections) {
  //       // std::cout << "road[" << road_ptr->getId() << "] section[" << section_ptr->getId()
  //       //           << "] lanes num = " << section_ptr->getLanes().size() << std::endl;
  //       const txLanes& lanes = section_ptr->getLanes();
  //       for (auto& lane_ptr : lanes) {
  //         PointVec points;
  //         if (!lane_ptr->getGeometry()) continue;
  //         dynamic_cast<const txLineCurve*>(lane_ptr->getGeometry())->getPoints(points);
  //         if (points.empty()) {
  //           std::cout << "lane[" << lane_ptr->getTxLaneId().roadId << "," << lane_ptr->getTxLaneId().sectionId << ","
  //                     << lane_ptr->getTxLaneId().laneId << "] points is empty\n";
  //         }
  //         auto coordtype = lane_ptr->getGeometry()->getCoordType();
  //         if (coordtype == COORD_WGS84) {
  //           for (auto& p : points) {
  //             lanegps << std::to_string(lane_ptr->getTxLaneId().roadId) + "_" +
  //                            std::to_string(lane_ptr->getTxLaneId().sectionId) + "_" +
  //                            std::to_string(lane_ptr->getTxLaneId().laneId)
  //                     << " " << p.x << " " << p.y << " " << p.z << std::endl;
  //           }
  //         }
  //       }
  //       const txLaneBoundaries& bdys = section_ptr->getBoundaries();
  //       for (auto& boundaryPtr : bdys) {
  //         PointVec points;
  //         dynamic_cast<const txLineCurve*>(boundaryPtr->getGeometry())->getPoints(points);

  //         auto coordtype = boundaryPtr->getGeometry()->getCoordType();
  //         if (coordtype == COORD_WGS84) {
  //           for (auto& p : points) {
  //             llinegps << std::to_string(section_ptr->getRoadId()) + "_" + std::to_string(section_ptr->getId()) << ""
  //                      << p.x << " " << p.y << " " << p.z << std::endl;
  //           }
  //         }
  //       }
  //     }
  //   }
  // }

  // lanegps.close();
  // llinegps.close();

  // hadmap::hadmapClose(&pHandle);
}
