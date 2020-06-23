#include <sys/time.h>

#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <ophidian/design/DesignFactory.h>
#include <ophidian/routing/ILPRouting.h>
#include <ophidian/parser/ICCAD2020Writer.h>

/*
void write_statistics_for_circuit(ophidian::design::Design & design, std::string circuit_name) {
    std::vector<ophidian::circuit::Net> nets(design.netlist().begin_net(), design.netlist().end_net());

    std::ofstream stats_file("stats/" + circuit_name + "_nets.csv");
    stats_file << "net,pins,stwl,routed_length,routed_length_no_vias,box_width,box_height" << std::endl;
    for (auto net : nets) {
        auto net_name = design.netlist().name(net);
        auto pins = design.netlist().pins(net);

        std::vector<ophidian::interconnection::Flute::Point> net_points;
        net_points.reserve(pins.size());
        auto min_x = std::numeric_limits<double>::max();
        auto max_x = -std::numeric_limits<double>::max();
        auto min_y = std::numeric_limits<double>::max();
        auto max_y = -std::numeric_limits<double>::max();
        for (auto pin : pins) {
            auto pin_location = design.placement().location(pin);
            net_points.push_back(pin_location);

            min_x = std::min(min_x, pin_location.x().value());
            max_x = std::max(max_x, pin_location.x().value());
            min_y = std::min(min_y, pin_location.y().value());
            max_y = std::max(max_y, pin_location.y().value());
        }

        auto & flute = ophidian::interconnection::Flute::instance();
        auto tree = flute.create(net_points);
        auto stwl = tree->length().value();
        stwl /= 10;
        if (stwl == 0) {
            stwl = 1;
        } 

        auto routes = design.global_routing().routes(net);
        auto routed_length_no_vias = 0;
        auto via_length = 0;
        for (auto route : routes) {
            auto start = design.global_routing().start(route);
            auto end = design.global_routing().end(route);
            routed_length_no_vias += (std::abs(start.x().value() - end.x().value()) + std::abs(start.y().value() - end.y().value()));

            auto start_layer = design.global_routing().start_layer(route);
            auto end_layer = design.global_routing().end_layer(route);
            auto start_layer_index = design.routing_library().layerIndex(start_layer);
            auto end_layer_index = design.routing_library().layerIndex(end_layer);
            via_length += std::abs(start_layer_index - end_layer_index);
        }
        routed_length_no_vias /= 10;
        if (routed_length_no_vias == 0) {
            routed_length_no_vias = 1;
        }
        auto routed_length = routed_length_no_vias + via_length;

        auto box_width = max_x - min_x;
        auto box_height = max_y - min_y;

        stats_file << net_name << "," << pins.size() << "," << stwl << "," << routed_length << "," << routed_length_no_vias << "," << box_width << "," << box_height << std::endl;
    }
    stats_file.close();
}
*/



void run_ilp_for_circuit(ophidian::design::Design & design, std::string circuit_name) {
    auto t1 = std::chrono::high_resolution_clock::now();

    std::cout << "create ilp routing" << std::endl;
    ophidian::routing::ILPRouting ilpRouting(design, circuit_name);
    std::cout << "create writer" << std::endl;
    ophidian::parser::ICCAD2020Writer iccad_output_writer(design, circuit_name);

    std::cout << "get nets" << std::endl;
    std::vector<ophidian::circuit::Net> nets(design.netlist().begin_net(), design.netlist().end_net());
    std::vector<ophidian::circuit::Net> fixed_nets;
    std::vector<ophidian::circuit::Net> routed_nets;
    
    //std::vector<ophidian::circuit::Net> nets = {design.netlist().find_net("net3148")};
    // std::vector<ophidian::circuit::Net> nets = {design.netlist().find_net("N2116")};

    std::cout << "# of nets " << nets.size() << std::endl;
    auto wlb = design.global_routing().wirelength_in_gcell(nets);
    auto ovfl = design.global_routing().is_overflow() ? "there is overflow" : "No overflow";
    std::cout << ovfl << "in input file" << std::endl;
    std::vector<std::pair<ophidian::routing::ILPRouting::cell_type, ophidian::routing::ILPRouting::point_type>> movements; 
    std::cout << "routing nets" << std::endl;
    auto result = ilpRouting.route_nets(nets, fixed_nets, routed_nets, movements);
    std::cout << "result " << result << std::endl;

    if(result){
        iccad_output_writer.write_ICCAD_2020_output("RUN_TESTS_OUTPUT.txt", movements);
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    auto runtime = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1).count();

    std::cout << "RUNTIME " << runtime << std::endl;


    ovfl = design.global_routing().is_overflow() ? "there is overflow" : "No overflow";
    std::cout << ovfl << std::endl;
    ophidian::routing::GlobalRouting::net_container_type ovfl_nets{};
    ovfl = design.global_routing().is_overflow(nets, ovfl_nets) ? "there is overflow" : "No overflow";
    std::cout << ovfl << std::endl;
    
    auto wla = design.global_routing().wirelength_in_gcell(nets);
    std::cout << "WL before: " << wlb << " WL after: " << wla << " change: " << wlb-wla << std::endl;
    //write_statistics_for_circuit(design, circuit_name);

    std::cout << "connected nets" << std::endl;
        for (auto net : nets) {
            auto net_name = design.netlist().name(net);
            ophidian::routing::GlobalRouting::gcell_container_type pin_gcells = {};
            for (auto pin : design.netlist().pins(net)) {
                auto pin_name = design.netlist().name(pin);                
                auto location = design.placement().location(pin);
                auto box = ophidian::routing::GCellGraph::box_type{location, location};
                auto pin_geometry = design.placement().geometry(pin);
                auto layer_name = pin_geometry.front().second;
                auto pin_layer = design.routing_library().find_layer_instance(layer_name);
                auto layer_index = design.routing_library().layerIndex(pin_layer);

                if (net_name == "net3148") {
                std::cout << "pin " << pin_name << " layer " << layer_name << " index " << layer_index << " location " << location.x().value() << "," << location.y().value() << std::endl;
                }

                design.global_routing().gcell_graph()->intersect(pin_gcells, box, layer_index-1);
            }
            auto connected = design.global_routing().is_connected(net, pin_gcells, net_name);

            if(!connected)
                std::cout << "net " << net_name << " is open" << std::endl;
        }
}

TEST_CASE("run ILP for iccad20 benchmarks", "[iccad20]") {
    std::vector<std::string> circuit_names = {
         "case1",
        //"case1N4",
        // "case2",
        // "case3",
        //"case3_no_blockages",
        // "case3_no_extra_demand"
        //"case3_only_same_grid"
        //"case3_only_adj_rule"
        //"case3_small_rules"
        //"same_grid_test"
    };

    std::string benchmarks_path = "./input_files/iccad2020/cases/";
    // std::string benchmarks_path = "./input_files/iccad20/";
    for (auto circuit_name : circuit_names) {
        std::cout << "running circuit " << circuit_name << std::endl;

        std::string iccad_2020_file = benchmarks_path + circuit_name + ".txt";

        std::cout << "file " << iccad_2020_file << std::endl;

        auto iccad_2020 = ophidian::parser::ICCAD2020{iccad_2020_file};

        auto design = ophidian::design::Design();
        ophidian::design::factory::make_design_iccad2020(design, iccad_2020);
        run_ilp_for_circuit(design, circuit_name);
        // auto is_connected = design.global_routing().is_connected() ? "grafo conexo" : "grafo desconexo!";
        
        // std::cout << "done, " << is_connected << circuit_name << std::endl;
    }


}

TEST_CASE("run ILP for iccad19 benchmarks", "[iccad20]") {
    std::vector<std::string> circuit_names = {
         "ispd18_test1",
         //"ispd19_test1",
    };

    std::string benchmarks_path = "./input_files/ispd19/";
    // std::string benchmarks_path = "./input_files/iccad20/";
    for (auto circuit_name : circuit_names) {
        std::cout << "running circuit " << circuit_name << std::endl;

         std::string def_file =   benchmarks_path + "/" + circuit_name + "/" + circuit_name + ".input.def";
         std::string lef_file =   benchmarks_path + "/" + circuit_name + "/" + circuit_name + ".input.lef";
         std::string guide_file = benchmarks_path + "/" + circuit_name + "/" + circuit_name + ".solution_cugr.guide";

         ophidian::parser::Def def;
         ophidian::parser::Lef lef;
         ophidian::parser::Guide guide;
         //#pragma omp parallel
         //{
             def = ophidian::parser::Def{def_file};
             lef = ophidian::parser::Lef{lef_file};
             guide = ophidian::parser::Guide{guide_file};
         //}

        auto design = ophidian::design::Design();
        ophidian::design::factory::make_design(design, def, lef, guide);
        run_ilp_for_circuit(design, circuit_name);
        // auto is_connected = design.global_routing().is_connected() ? "grafo conexo" : "grafo desconexo!";
        
        // std::cout << "done, " << is_connected << circuit_name << std::endl;
    }


}

TEST_CASE("iccad20 case 3 no extra demand benchmark", "[iccad20case3]") {
    std::string circuit_name = "case3_no_extra_demand";
    std::string benchmarks_path = "./input_files/iccad20/";
    std::string iccad_2020_file = benchmarks_path + circuit_name + ".txt";
    auto iccad_2020 = ophidian::parser::ICCAD2020{iccad_2020_file};
    auto design = ophidian::design::Design();
    ophidian::design::factory::make_design_iccad2020(design, iccad_2020);

    ophidian::routing::ILPRouting ilpRouting(design, circuit_name);
    ophidian::parser::ICCAD2020Writer iccad_output_writer(design, circuit_name);
    auto & global_routing = design.global_routing();

    using tuple = std::tuple<int, int, int>;
    std::vector<std::pair<tuple, tuple>> initial_segments;
    initial_segments.push_back(std::make_pair(std::make_tuple(21, 18, 2), std::make_tuple(21, 19, 2)));
    initial_segments.push_back(std::make_pair(std::make_tuple(21, 17, 2), std::make_tuple(21, 18, 2)));
    initial_segments.push_back(std::make_pair(std::make_tuple(21, 19, 1), std::make_tuple(21, 19, 2)));
    initial_segments.push_back(std::make_pair(std::make_tuple(21, 17, 1), std::make_tuple(21, 17, 2)));
    initial_segments.push_back(std::make_pair(std::make_tuple(21, 18, 1), std::make_tuple(21, 18, 2)));
    initial_segments.push_back(std::make_pair(std::make_tuple(21, 17, 1), std::make_tuple(22, 17, 1)));
    initial_segments.push_back(std::make_pair(std::make_tuple(21, 18, 1), std::make_tuple(22, 18, 1)));

    auto net = design.netlist().find_net("N2548");
    auto segments = global_routing.segments(net);

    std::vector<std::pair<tuple, tuple>> gr_segments;
    for(auto segment : segments){
        auto box = global_routing.box(segment);
        auto start = box.min_corner();
        auto end = box.max_corner();
        auto start_layer = global_routing.layer_start(segment);
        auto end_layer = global_routing.layer_end(segment);
        // auto gcell_start = global_routing.gcell_start(segment);
        // auto gcell_end = global_routing.gcell_end(segment);
        auto start_layer_index = design.routing_library().layerIndex(start_layer);
        auto end_layer_index = design.routing_library().layerIndex(end_layer);

        gr_segments.push_back(std::make_pair(
            std::make_tuple( (((int)start.x().value() - 5) / 10 +1), (((int)start.y().value() - 5) / 10 +1), start_layer_index),
            std::make_tuple( (((int)end.x().value() - 5) / 10 +1), (((int)end.y().value() - 5) / 10 +1), end_layer_index)));
    }

    // TODO: compare if initial_segments and gr_segments are equal!


    std::vector<ophidian::circuit::Net> nets = {design.netlist().find_net("N2548")};
    std::vector<std::pair<ophidian::routing::ILPRouting::cell_type, ophidian::routing::ILPRouting::point_type>> movements; 
    
    std::vector<ophidian::circuit::Net> fixed_nets;
    std::vector<ophidian::circuit::Net> routed_nets;
    
    std::cout << "routing nets" << std::endl;
    auto result = ilpRouting.route_nets(nets, fixed_nets, routed_nets, movements);
    std::cout << "result " << result << std::endl;

    if(result){
        iccad_output_writer.write_ICCAD_2020_output("RUN_TESTS_OUTPUT.txt", movements);
    }

    int wirelength = 0;
    for(auto net : nets){
        auto gcells = global_routing.gcells(net);
        auto net_name = design.netlist().name(net);
        wirelength += gcells.size();
        std::cout << "Net: " << net_name << " = " << gcells.size() << std::endl;
    }
    std::cout << "wirelength : " << wirelength << std::endl;
}

/*
TEST_CASE("write statistics for iccad20 benchmarks", "[iccad20]") {
    std::vector<std::string> circuit_names = {
        "case1",
        "case2",
        "case3",
    };
    std::string benchmarks_path = "./input_files/iccad2020/cases/";

    for (auto circuit_name : circuit_names) {
        std::cout << "running circuit " << circuit_name << std::endl;

        std::string iccad_2020_file = benchmarks_path + circuit_name + ".txt";

        auto iccad_2020 = ophidian::parser::ICCAD2020{iccad_2020_file};

        auto design = ophidian::design::Design();
        ophidian::design::factory::make_design_iccad2020(design, iccad_2020);

        write_statistics_for_circuit(design, circuit_name);
    }
}
*/
