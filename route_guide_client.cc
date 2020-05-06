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

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "helper.h"
#include "protos/route_guide.grpc.pb.h"

routeguide::Point MakePoint(long latitude, long longitude)
{
    routeguide::Point p;
    p.set_latitude(latitude);
    p.set_longitude(longitude);
    return p;
}

routeguide::Feature MakeFeature(const std::string &name, long latitude, long longitude)
{
    routeguide::Feature f;
    f.set_name(name);
    f.mutable_location()->CopyFrom(MakePoint(latitude, longitude));
    return f;
}

routeguide::RouteNote MakeRouteNote(const std::string &message, long latitude,
                        long longitude)
{
    routeguide::RouteNote n;
    n.set_message(message);
    n.mutable_location()->CopyFrom(MakePoint(latitude, longitude));
    return n;
}

class RouteGuideClient
{
public:
    RouteGuideClient(std::shared_ptr<grpc::Channel> channel,
                     const std::string &db)
        : stub_(routeguide::RouteGuide::NewStub(channel))
    {
        helper::ParseDb(db, &feature_list_);
    }

    void GetFeature()
    {
        routeguide::Feature feature;
        auto point = MakePoint(409146138, -746188906);
        GetOneFeature(point, &feature);
        point = MakePoint(0, 0);
        GetOneFeature(point, &feature);
    }

    void ListFeatures()
    {
        routeguide::Rectangle rect;
        routeguide::Feature feature;
        grpc::ClientContext context;

        rect.mutable_lo()->set_latitude(400000000);
        rect.mutable_lo()->set_longitude(-750000000);
        rect.mutable_hi()->set_latitude(420000000);
        rect.mutable_hi()->set_longitude(-730000000);
        std::cout << "Looking for features between 40, -75 and 42, -73"
                  << std::endl;

        std::unique_ptr<grpc::ClientReader<routeguide::Feature>> reader(
            stub_->ListFeatures(&context, rect));
        while (reader->Read(&feature))
        {
            std::cout << "Found feature called " << feature.name() << " at "
                      << feature.location().latitude() / kCoordFactor_ << ", "
                      << feature.location().longitude() / kCoordFactor_
                      << std::endl;
        }
        auto status = reader->Finish();
        if (status.ok())
        {
            std::cout << "ListFeatures rpc succeeded." << std::endl;
        }
        else
        {
            std::cout << "ListFeatures rpc failed." << std::endl;
        }
    }

    void RecordRoute()
    {
        routeguide::Point point;
        routeguide::RouteSummary stats;
        grpc::ClientContext context;
        const int kPoints = 10;
        unsigned seed =
            std::chrono::system_clock::now().time_since_epoch().count();

        std::default_random_engine generator(seed);
        std::uniform_int_distribution<int> feature_distribution(
            0, feature_list_.size() - 1);
        std::uniform_int_distribution<int> delay_distribution(500, 1500);

        std::unique_ptr<grpc::ClientWriter<routeguide::Point>> writer(
            stub_->RecordRoute(&context, &stats));
        for (int i = 0; i < kPoints; i++)
        {
            const auto &f = feature_list_[feature_distribution(generator)];
            std::cout << "Visiting point "
                      << f.location().latitude() / kCoordFactor_ << ", "
                      << f.location().longitude() / kCoordFactor_ << std::endl;
            if (!writer->Write(f.location()))
            {
                // Broken stream.
                break;
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds(delay_distribution(generator)));
        }
        writer->WritesDone();
        auto status = writer->Finish();
        if (status.ok())
        {
            std::cout << "Finished trip with " << stats.point_count()
                      << " points\n"
                      << "Passed " << stats.feature_count() << " features\n"
                      << "Travelled " << stats.distance() << " meters\n"
                      << "It took " << stats.elapsed_time() << " seconds"
                      << std::endl;
        }
        else
        {
            std::cout << "RecordRoute rpc failed." << std::endl;
        }
    }

    void RouteChat()
    {
        grpc::ClientContext context;

        std::shared_ptr<grpc::ClientReaderWriter<routeguide::RouteNote, routeguide::RouteNote>> stream(
            stub_->RouteChat(&context));

        std::thread writer([stream]() {
            std::vector<routeguide::RouteNote> notes{MakeRouteNote("First message", 0, 0),
                                         MakeRouteNote("Second message", 0, 1),
                                         MakeRouteNote("Third message", 1, 0),
                                         MakeRouteNote("Fourth message", 0, 0)};
            for (const auto &note : notes)
            {
                std::cout << "Sending message " << note.message() << " at "
                          << note.location().latitude() << ", "
                          << note.location().longitude() << std::endl;
                stream->Write(note);
            }
            stream->WritesDone();
        });

        routeguide::RouteNote server_note;
        while (stream->Read(&server_note))
        {
            std::cout << "Got message " << server_note.message() << " at "
                      << server_note.location().latitude() << ", "
                      << server_note.location().longitude() << std::endl;
        }
        writer.join();
        auto status = stream->Finish();
        if (!status.ok())
        {
            std::cout << "RouteChat rpc failed." << std::endl;
        }
    }

private:
    bool GetOneFeature(const routeguide::Point &point, routeguide::Feature *feature)
    {
        grpc::ClientContext context;
        auto status = stub_->GetFeature(&context, point, feature);
        if (!status.ok())
        {
            std::cout << "GetFeature rpc failed." << std::endl;
            return false;
        }
        if (!feature->has_location())
        {
            std::cout << "Server returns incomplete feature." << std::endl;
            return false;
        }
        if (feature->name().empty())
        {
            std::cout << "Found no feature at "
                      << feature->location().latitude() / kCoordFactor_ << ", "
                      << feature->location().longitude() / kCoordFactor_
                      << std::endl;
        }
        else
        {
            std::cout << "Found feature called " << feature->name() << " at "
                      << feature->location().latitude() / kCoordFactor_ << ", "
                      << feature->location().longitude() / kCoordFactor_
                      << std::endl;
        }
        return true;
    }

    const float kCoordFactor_ = 10000000.0;
    std::unique_ptr<routeguide::RouteGuide::Stub> stub_;
    std::vector<routeguide::Feature> feature_list_;
};

int main(int argc, char **argv)
{
    // Expect only arg: --db_path=path/to/route_guide_db.json.
    std::string db = helper::GetDbFileContent(argc, argv);
    RouteGuideClient guide(
        grpc::CreateChannel("localhost:50051",
                            grpc::InsecureChannelCredentials()),
        db);

    std::cout << "-------------- GetFeature --------------" << std::endl;
    guide.GetFeature();
    std::cout << "-------------- ListFeatures --------------" << std::endl;
    guide.ListFeatures();
    std::cout << "-------------- RecordRoute --------------" << std::endl;
    guide.RecordRoute();
    std::cout << "-------------- RouteChat --------------" << std::endl;
    guide.RouteChat();

    return 0;
}
