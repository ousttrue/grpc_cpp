/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include "helper.h"
#include "protos/route_guide.grpc.pb.h"

float ConvertToRadians(float num)
{
    return num * 3.1415926 / 180;
}

// The formula is based on http://mathforum.org/library/drmath/view/51879.html
float GetDistance(const routeguide::Point &start, const routeguide::Point &end)
{
    const float kCoordFactor = 10000000.0;
    float lat_1 = start.latitude() / kCoordFactor;
    float lat_2 = end.latitude() / kCoordFactor;
    float lon_1 = start.longitude() / kCoordFactor;
    float lon_2 = end.longitude() / kCoordFactor;
    float lat_rad_1 = ConvertToRadians(lat_1);
    float lat_rad_2 = ConvertToRadians(lat_2);
    float delta_lat_rad = ConvertToRadians(lat_2 - lat_1);
    float delta_lon_rad = ConvertToRadians(lon_2 - lon_1);

    float a = pow(sin(delta_lat_rad / 2), 2) +
              cos(lat_rad_1) * cos(lat_rad_2) * pow(sin(delta_lon_rad / 2), 2);
    float c = 2 * atan2(sqrt(a), sqrt(1 - a));
    int R = 6371000; // metres

    return R * c;
}

std::string GetFeatureName(const routeguide::Point &point,
                           const std::vector<routeguide::Feature> &feature_list)
{
    for (const auto &f : feature_list)
    {
        if (f.location().latitude() == point.latitude() &&
            f.location().longitude() == point.longitude())
        {
            return f.name();
        }
    }
    return "";
}

class RouteGuideImpl final : public routeguide::RouteGuide::Service
{
public:
    explicit RouteGuideImpl(const std::string &db)
    {
        helper::ParseDb(db, &feature_list_);
    }

    grpc::Status GetFeature(grpc::ServerContext *context,
                            const routeguide::Point *point,
                            routeguide::Feature *feature) override
    {
        feature->set_name(GetFeatureName(*point, feature_list_));
        feature->mutable_location()->CopyFrom(*point);
        return grpc::Status::OK;
    }

    grpc::Status
    ListFeatures(grpc::ServerContext *context,
                 const routeguide::Rectangle *rectangle,
                 grpc::ServerWriter<routeguide::Feature> *writer) override
    {
        auto lo = rectangle->lo();
        auto hi = rectangle->hi();
        long left = (std::min)(lo.longitude(), hi.longitude());
        long right = (std::max)(lo.longitude(), hi.longitude());
        long top = (std::max)(lo.latitude(), hi.latitude());
        long bottom = (std::min)(lo.latitude(), hi.latitude());
        for (const auto &f : feature_list_)
        {
            if (f.location().longitude() >= left &&
                f.location().longitude() <= right &&
                f.location().latitude() >= bottom &&
                f.location().latitude() <= top)
            {
                writer->Write(f);
            }
        }
        return grpc::Status::OK;
    }

    grpc::Status RecordRoute(grpc::ServerContext *context,
                             grpc::ServerReader<routeguide::Point> *reader,
                             routeguide::RouteSummary *summary) override
    {
        routeguide::Point point;
        int point_count = 0;
        int feature_count = 0;
        float distance = 0.0;
        routeguide::Point previous;

        auto start_time = std::chrono::system_clock::now();
        while (reader->Read(&point))
        {
            point_count++;
            if (!GetFeatureName(point, feature_list_).empty())
            {
                feature_count++;
            }
            if (point_count != 1)
            {
                distance += GetDistance(previous, point);
            }
            previous = point;
        }
        auto end_time = std::chrono::system_clock::now();
        summary->set_point_count(point_count);
        summary->set_feature_count(feature_count);
        summary->set_distance(static_cast<long>(distance));
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(
            end_time - start_time);
        summary->set_elapsed_time(secs.count());

        return grpc::Status::OK;
    }

    grpc::Status RouteChat(
        grpc::ServerContext *context,
        grpc::ServerReaderWriter<routeguide::RouteNote, routeguide::RouteNote>
            *stream) override
    {
        routeguide::RouteNote note;
        while (stream->Read(&note))
        {
            std::unique_lock<std::mutex> lock(mu_);
            for (const auto &n : received_notes_)
            {
                if (n.location().latitude() == note.location().latitude() &&
                    n.location().longitude() == note.location().longitude())
                {
                    stream->Write(n);
                }
            }
            received_notes_.push_back(note);
        }

        return grpc::Status::OK;
    }

private:
    std::vector<routeguide::Feature> feature_list_;
    std::mutex mu_;
    std::vector<routeguide::RouteNote> received_notes_;
};

void RunServer(const std::string &db_path)
{
    std::string server_address("0.0.0.0:50051");
    RouteGuideImpl service(db_path);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char **argv)
{
    // Expect only arg: --db_path=path/to/route_guide_db.json.
    std::string db = helper::GetDbFileContent(argc, argv);
    RunServer(db);

    return 0;
}
