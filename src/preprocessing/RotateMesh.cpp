//  Copyright 2016 National Renewable Energy Laboratory
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#include "RotateMesh.h"
#include "core/ClassRegistry.h"

#include <Shards_BasicTopologies.hpp>

#include <stk_util/parallel/Parallel.hpp>
#include <stk_util/parallel/BroadcastArg.hpp>
#include <stk_util/parallel/ParallelReduce.hpp>

#include <stk_mesh/base/FindRestriction.hpp>
#include <stk_mesh/base/MetaData.hpp>
#include <stk_mesh/base/BulkData.hpp>
#include <stk_mesh/base/Entity.hpp>
#include <stk_mesh/base/Field.hpp>
#include <stk_mesh/base/CoordinateSystems.hpp>
#include <stk_mesh/base/Comm.hpp>

#include <vector>
#include <string>
#include <cmath>

namespace sierra {
namespace nalu {

REGISTER_DERIVED_CLASS(PreProcessingTask, RotateMesh, "rotate_mesh");

RotateMesh::RotateMesh(
    CFDMesh& mesh,
    const YAML::Node& node
) : PreProcessingTask(mesh),
    meta_(mesh.meta()),
    bulk_(mesh.bulk()),
    meshPartNames_(),
    meshParts_()
{
    load(node);
}

void RotateMesh::load(const YAML::Node& node)
{
    const auto& fParts = node["mesh_parts"];
    if (fParts.Type() == YAML::NodeType::Scalar) {
        meshPartNames_.push_back(fParts.as<std::string>());
    } else {
        meshPartNames_ = fParts.as<std::vector<std::string>>();
    }

    angle_ = node["angle"].as<double>();
    angle_ *= std::acos(-1.0) / 180.0;
    axis_ = node["axis"].as<std::vector<double>>();
    origin_ = node["origin"].as<std::vector<double>>();
}

void RotateMesh::initialize()
{
    for (auto pName: meshPartNames_){
        stk::mesh::Part* part = meta_.get_part(pName);
        if (NULL == part) {
            throw std::runtime_error(
                "RotateMesh: Mesh realm not found in mesh database.");
        } else {
            meshParts_.push_back(part);
        }
    }
}

void RotateMesh::run()
{
    if (bulk_.parallel_rank() == 0)
        std::cerr << "Rotating mesh " << std::endl;
    VectorFieldType* coords = meta_.get_field<VectorFieldType>(
        stk::topology::NODE_RANK, "coordinates");

    stk::mesh::Selector s_part = stk::mesh::selectUnion(meshParts_);
    const stk::mesh::BucketVector& node_buckets = bulk_.get_buckets(
        stk::topology::NODE_RANK, s_part);

    const double cosang = std::cos(0.5*angle_);
    const double sinang = std::sin(0.5*angle_);
    const double q0 = cosang;
    const double q1 = sinang * axis_[0];
    const double q2 = sinang * axis_[1];
    const double q3 = sinang * axis_[2];

    for(auto b: node_buckets) {
        for(size_t in=0; in < b->size(); in++) {
            auto node = (*b)[in];
            double* xyz = stk::mesh::field_data(*coords, node);

            const double cx = xyz[0] - origin_[0];
            const double cy = xyz[1] - origin_[1];
            const double cz = xyz[2] - origin_[2];

            xyz[0] = (q0*q0 + q1*q1 - q2*q2 - q3*q3) * cx +
                2.0 * (q1*q2 - q0*q3) * cy +
                2.0 * (q0*q2 + q1*q3) * cz + origin_[0];

            xyz[1] = 2.0 * (q1*q2 + q0*q3) * cx +
                (q0*q0 - q1*q1 + q2*q2 - q3*q3) * cy +
                2.0 * (q2*q3 - q0*q1) * cz + origin_[1];

            xyz[2] = 2.0 * (q1*q3 - q0*q2) * cx +
                2.0 * (q0*q1 + q2*q3) * cy +
                (q0*q0 - q1*q1 - q2*q2 + q3*q3) * cz + origin_[2];
        }
    }
}

}  // nalu
}  // sierra
