#include <iostream>
#include <ophidian/design/DesignFactory.h>
#include <ophidian/routing/ILPRouting.h>
#include <ophidian/routing/AStarRouting.h>
#include <ophidian/parser/ICCAD2020Writer.h>
#include <ophidian/util/log.h>
#include <ophidian/routing/MCFMultiThreading.h>
#include <chrono>

using namespace std;
using namespace ophidian::util;
// int THREADS_DEFAULT_VALUE = 1;

// std::chrono::time_point<std::chrono::steady_clock> start_time;
using gcell_type = ophidian::routing::GlobalRouting::gcell_type;

void calculate_median_gcell(ophidian::design::Design & design, ophidian::circuit::CellInstance & cell, std::vector<gcell_type> & target_gcells)
{
    using unit_type = ophidian::util::database_unit_t;
    using point_type = ophidian::util::LocationDbu;
    using net_type = ophidian::circuit::Net;

    auto & netlist = design.netlist();
    auto & placement = design.placement();
    std::vector<net_type> cell_nets;
    
    std::vector<double> x_positions;
    std::vector<double> y_positions;

    for(auto pin : netlist.pins(cell)){
        auto net = netlist.net(pin);
        if(net == ophidian::circuit::Net())
            continue;
        cell_nets.push_back(net);
        auto x_min = std::numeric_limits<double>::max();
        auto y_min = std::numeric_limits<double>::max();
        auto x_max = -std::numeric_limits<double>::max();
        auto y_max = -std::numeric_limits<double>::max();
        for(auto net_pin : netlist.pins(net)){
            if(net_pin == pin)
                continue;
            auto location = placement.location(net_pin);
            x_min = std::min(x_min, location.x().value());
            y_min = std::min(y_min, location.y().value());
            x_max = std::max(x_max, location.x().value());
            y_max = std::max(y_max, location.y().value());
            //x_positions.push_back(location.x().value());
            //y_positions.push_back(location.y().value());
        }
        x_positions.push_back(x_min);
        x_positions.push_back(x_max);
        y_positions.push_back(y_min);
        y_positions.push_back(y_max);
    }
    auto cell_location = placement.location(cell);
    auto current_gcell = design.global_routing().gcell_graph()->nearest_gcell(cell_location, 0);

    if(x_positions.empty() || y_positions.empty()) {
        //return current_gcell;
        return;
    }

    std::nth_element(x_positions.begin(), x_positions.begin() + x_positions.size()/2, x_positions.end());
    auto median_x = x_positions[x_positions.size()/2];
    //auto median_x_left = x_positions[x_positions.size()/2];
    //auto median_x_right = x_positions[x_positions.size()/2 + 1];
    std::nth_element(y_positions.begin(), y_positions.begin() + y_positions.size()/2, y_positions.end());
    auto median_y = y_positions[y_positions.size()/2];
    //auto median_y_left = y_positions[y_positions.size()/2];
    //auto median_y_right = y_positions[y_positions.size()/2 + 1];

    //point_type median_point {unit_type(median_x), unit_type(median_y)};
    std::vector<point_type> median_points;
    median_points.push_back({unit_type{median_x}, unit_type{median_y}});
    /*median_points.push_back({unit_type{median_x+10}, unit_type{median_y}});
    median_points.push_back({unit_type{median_x-10}, unit_type{median_y}});
    median_points.push_back({unit_type{median_x}, unit_type{median_y+10}});
    median_points.push_back({unit_type{median_x}, unit_type{median_y-10}});*/
    /*median_points.push_back({unit_type{median_x_left}, unit_type{median_y_left}});
    median_points.push_back({unit_type{median_x_right}, unit_type{median_y_left}});
    median_points.push_back({unit_type{median_x_left}, unit_type{median_y_right}});
    median_points.push_back({unit_type{median_x_right}, unit_type{median_y_right}});*/

    for (auto median_point : median_points) {
        auto nearest_gcell = design.global_routing().gcell_graph()->nearest_gcell(median_point, 0);
        
        target_gcells.push_back(nearest_gcell);
    }

    /*if(current_gcell != nearest_gcell){
        return nearest_gcell;
    }
    return current_gcell;*/
}

double test_target_gcell(ophidian::design::Design & design, ophidian::circuit::CellInstance & cell, gcell_type & initial_gcell, gcell_type & target_gcell, ophidian::routing::AStarRouting & astar_routing) {
    using unit_type = ophidian::util::database_unit_t;
    using point_type = ophidian::util::LocationDbu;
    using net_type = ophidian::circuit::Net;
    using AStarSegment = ophidian::routing::AStarSegment;

    auto& global_routing = design.global_routing();
    auto& netlist = design.netlist();
    auto& placement = design.placement();
    auto& routing_constr = design.routing_constraints();
    auto& std_cells = design.standard_cells();
    auto gcell_graph_ptr = global_routing.gcell_graph();

    // Get connected nets
    std::vector<net_type> cell_nets;
    for(auto pin : netlist.pins(cell))
    {
        auto net = netlist.net(pin);
        if(net == ophidian::circuit::Net())
            continue;
        cell_nets.push_back(net);
    }

    //move cell to median
    auto overflow_movement = global_routing.move_cell(initial_gcell, target_gcell, cell, netlist, placement, routing_constr, std_cells);
    if(overflow_movement == false)
    {
        std::vector<AStarSegment> segments;
        bool routed_all_nets = true;
        for(auto net : cell_nets)
        {
            routed_all_nets = astar_routing.route_net(net, segments, false);
            if(routed_all_nets == false)
                break;
        }
        if(routed_all_nets)
        {
            bool working_correct = astar_routing.apply_segments_to_global_routing(segments);
            auto wirelength_after = global_routing.wirelength(cell_nets);
            if(working_correct) {
                return wirelength_after;
            }
        }
    }
    
    return std::numeric_limits<double>::max();
}

bool move_cell(ophidian::design::Design & design, ophidian::circuit::CellInstance & cell, ophidian::routing::AStarRouting & astar_routing)
{
    using unit_type = ophidian::util::database_unit_t;
    using point_type = ophidian::util::LocationDbu;
    using net_type = ophidian::circuit::Net;
    using AStarSegment = ophidian::routing::AStarSegment;

    auto& global_routing = design.global_routing();
    auto& netlist = design.netlist();
    auto& placement = design.placement();
    auto& routing_constr = design.routing_constraints();
    auto& std_cells = design.standard_cells();
    auto gcell_graph_ptr = global_routing.gcell_graph();

    auto initial_location = placement.location(cell);
    auto initial_gcell = gcell_graph_ptr->nearest_gcell(initial_location, 0);
    std::vector<gcell_type> target_gcells;
    //auto median_gcell = calculate_median_gcell(design, cell);
    calculate_median_gcell(design, cell, target_gcells);

    auto debug_gcell = design.global_routing().gcell_graph()->gcell(20, 19, 0);
    auto capacity = design.global_routing().gcell_graph()->capacity(debug_gcell);
    auto demand = design.global_routing().gcell_graph()->demand(debug_gcell);
    auto layer_index = design.global_routing().gcell_graph()->layer_index(debug_gcell);
    auto gcell_box = design.global_routing().gcell_graph()->box(debug_gcell);

    // Get connected nets
    std::vector<net_type> cell_nets;
    for(auto pin : netlist.pins(cell))
    {
        auto net = netlist.net(pin);
        if(net == ophidian::circuit::Net())
            continue;
        cell_nets.push_back(net);

        /*for(auto gcell : global_routing.gcells(net)) {
                auto gcell_box = gcell_graph_ptr->box(gcell);
                auto layer_index = gcell_graph_ptr->layer_index(gcell);
                std::cout << "initial gcell " << gcell_box.min_corner().y().value() << " " << gcell_box.min_corner().x().value() << " " << layer_index << std::endl;

                auto capacity = gcell_graph_ptr->capacity(gcell);
                auto demand = gcell_graph_ptr->demand(gcell);
                std::cout << "capacity " << capacity << " demand " << demand << std::endl;
            }*/

    }
    // Backup routing information
    std::vector<AStarSegment> initial_segments;
    auto wirelength_before = global_routing.wirelength(cell_nets);
    //std::cout << "initial gcells " << std::endl;
    for(auto net : cell_nets)
    {
        for(auto segment : global_routing.segments(net)) {
            initial_segments.push_back(AStarSegment(global_routing.box(segment), global_routing.layer_start(segment), global_routing.layer_end(segment), net));
        }
    }

    auto min_wirelength = wirelength_before;
    auto best_gcell = gcell_type{};
    for (auto target_gcell : target_gcells) {
        if (initial_gcell != target_gcell) {
            for (auto net : cell_nets) {
                global_routing.unroute(net);
            }

            auto wirelength = test_target_gcell(design, cell, initial_gcell, target_gcell, astar_routing);
            if (wirelength < min_wirelength) {
                min_wirelength = wirelength;
                best_gcell = target_gcell;
            }
            
            /*capacity = design.global_routing().gcell_graph()->capacity(debug_gcell);
            demand = design.global_routing().gcell_graph()->demand(debug_gcell);
            std::cout << "debug gcell after movement test capacity " << capacity << " demand " << demand << std::endl;*/

            for(auto net : cell_nets) {
                global_routing.unroute(net);
            }

            bool undo_overflow = global_routing.move_cell(target_gcell, initial_gcell, cell, netlist, placement, routing_constr, std_cells);
            if(undo_overflow == true) {
                std::cout<<"WARNING: UNDO MOVEMENT OVERFLOW!"<<std::endl;
            }
            bool undo = astar_routing.apply_segments_to_global_routing(initial_segments);//This should never fail
            if(undo == false) {
                std::cout<<"WARNING: UNDO ROUTING OVERFLOW!"<<std::endl;
            }
            
            /*capacity = design.global_routing().gcell_graph()->capacity(debug_gcell);
            demand = design.global_routing().gcell_graph()->demand(debug_gcell);
            std::cout << "debug gcell after movement undo capacity " << capacity << " demand " << demand << std::endl;*/
        }
    }
    if (best_gcell != gcell_type{}) {
        for (auto net : cell_nets) {
            global_routing.unroute(net);
        }
        auto result = test_target_gcell(design, cell, initial_gcell, best_gcell, astar_routing);
        if (result == false) {
            std::cout << "WARNING: FAILED TO MOVE TO BEST GCELL" << std::endl;
        }
        return true;
    } else {
        //std::cout << "could not find best gcell" << std::endl;
    }
    return false;
}

bool cell_has_more_than_1_pin_in_same_net(ophidian::design::Design & design, ophidian::circuit::CellInstance cell)
{
    auto & netlist = design.netlist();
    std::unordered_map<std::string, int> pin_map;
    for(auto pin : netlist.pins(cell))
    {
        auto net = netlist.net(pin);
        auto net_name = netlist.name(net);
        pin_map[net_name]++;
    }
    for(auto map_it : pin_map)
        if(map_it.second > 1)
            return true;
    return false;
}

std::vector<std::pair<ophidian::circuit::CellInstance, double>> compute_cell_move_costs_descending_order(ophidian::design::Design & design)
{
    auto & netlist = design.netlist();
    auto & placement = design.placement();
    std::vector<std::pair<ophidian::circuit::CellInstance, double>> cells_costs;

    for(auto cell_it = netlist.begin_cell_instance(); cell_it != netlist.end_cell_instance(); ++cell_it)
    {
        auto cell = *cell_it;
        if(placement.isFixed(cell))
            continue;
        if(cell_has_more_than_1_pin_in_same_net(design, cell))
            continue;

        std::unordered_set<ophidian::circuit::Net, ophidian::entity_system::EntityBaseHash> cell_nets;
        for(auto pin : netlist.pins(cell))
        {
            auto net = netlist.net(pin);
            cell_nets.insert(net);
        }

        double routed_length = 0;
        for(auto net : cell_nets)
            routed_length += design.global_routing().wirelength(net);
        auto cost = routed_length;
        cells_costs.push_back(std::make_pair(cell, cost));
    }
    //SORT IN DESCENDING ORDER
    std::sort(cells_costs.begin(), cells_costs.end(), [](std::pair<ophidian::circuit::CellInstance, double> cost_a, std::pair<ophidian::circuit::CellInstance, double> cost_b) {return cost_a.second > cost_b.second;});
    return cells_costs;
}

void move_cells_for_until_x_minutes(ophidian::design::Design & design,
                                    int time_limit,
                                    std::vector<std::pair<ophidian::circuit::CellInstance, double>> & cells,
                                    std::vector<std::pair<ophidian::circuit::CellInstance, ophidian::util::LocationDbu>> & movements,
                                    ophidian::routing::AStarRouting & astar_routing)
{
    /*auto debug_gcell = design.global_routing().gcell_graph()->gcell(20, 19, 0);
    auto capacity = design.global_routing().gcell_graph()->capacity(debug_gcell);
    auto demand = design.global_routing().gcell_graph()->demand(debug_gcell);
    auto layer_index = design.global_routing().gcell_graph()->layer_index(debug_gcell);
    auto gcell_box = design.global_routing().gcell_graph()->box(debug_gcell);

    std::cout << "debug gcell " << gcell_box.min_corner().y().value() << " " << gcell_box.min_corner().x().value() << " " << layer_index << std::endl;
    std::cout << "debug gcell capacity " << capacity << " demand " << demand << std::endl;*/

    auto nets = std::vector<ophidian::circuit::Net>{design.netlist().begin_net(), design.netlist().end_net()};
    //auto wirelength = design.global_routing().wirelength(nets);
    //std::cout << "initial wirelength " << wirelength << std::endl;

    auto start_time = std::chrono::steady_clock::now();
    int moved_cells = movements.size();
    for(auto pair : cells)
    {
        auto cell = pair.first;
        if (design.placement().isFixed(cell)) {
            continue;
        }
        auto cell_name = design.netlist().name(cell);
        //std::cout << "cell " << cell_name << std::endl;
        auto moved = move_cell(design, cell, astar_routing);
        if(moved)
        {
            auto location = design.placement().location(cell);
            moved_cells++;            
            movements.push_back(std::make_pair(cell, design.placement().location(cell)));
            //std::cout << "moved to " << location.x().value() << "," << location.y().value() << std::endl;
            //std::cout<<"# of moved cells: "<<moved_cells<<std::endl;

            //auto wirelength = design.global_routing().wirelength(nets);
            //std::cout << "wirelength " << wirelength << std::endl;
        }
    
        /*auto capacity = design.global_routing().gcell_graph()->capacity(debug_gcell);
        auto demand = design.global_routing().gcell_graph()->demand(debug_gcell);
        std::cout << "debug gcell " << gcell_box.min_corner().y().value() << " " << gcell_box.min_corner().x().value() << " " << layer_index << std::endl;
        std::cout << "debug gcell capacity " << capacity << " demand " << demand << std::endl;*/
        auto end_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> diff = end_time-start_time;
        //std::cout << "time " << diff.count() << std::endl;
        bool time_out = diff.count() > time_limit * 60.0 ? true : false;
        //if (cell_name == "C1245") break;
        //if(moved_cells == design.routing_constraints().max_cell_movement() || time_out)
        if(moved_cells == design.routing_constraints().max_cell_movement() || time_out)
        //if(moved_cells == 1)
            break;
    }
}

void greetings(){
    using std::endl;

    printlog("===================================================");
    printlog("              ICCAD 2020 CAD Contest               ");
    printlog("      Problem B : Routing with Cell Movement       ");
    printlog("            Team Number:   cada0194                ");
    printlog("              Team Name:      CABRA                ");
    printlog("                    Members:                       ");
    printlog("Affiliation:  University of Calgary                ");
    printlog("    Erfan Aghaeekiasaraee                          ");
    printlog("    Upma Gandhi                                    ");
    printlog("    Laleh Behjat                                   ");
    printlog("Affiliation:  Federal University of Santa Catarina ");
    printlog("    Arthur Philippi Bianco                         ");
    printlog("    Renan Oliveira Netto                           ");
    printlog("    Sheiny Fabre Almeida                           ");
    printlog("    Tiago Augusto Fontana                          ");
    printlog("    Vinicius Livramento                            ");
    printlog("    Cristina Meinhardt                             ");
    printlog("    Jose Luis Guntzel                              ");
    printlog("===================================================");
};

void run_mcf_for_circuit(ophidian::design::Design & design, std::string circuit_name, std::string output){
    /*ophidian::routing::AStarRouting astar_routing{design};
    auto& netlist = design.netlist();
    for(auto net_it = netlist.begin_net(); net_it != netlist.end_net(); net_it++) {
        auto net = *net_it;
        auto net_name = design.netlist().name(net);
        std::cout << "net " << net_name << std::endl;
        design.global_routing().unroute(net);
        std::vector<ophidian::routing::AStarSegment> segments;    
        astar_routing.route_net(net, segments);
    }*/

    // UCal::MCFRouting mcf_routing(design,circuit_name);
    
    auto start_time = std::chrono::steady_clock::now();

    std::vector<std::pair<ophidian::routing::ILPRouting<IloBoolVar>::cell_type, ophidian::routing::ILPRouting<IloBoolVar>::point_type>> movements;

    UCal::MCFMultiThreading mcf_multi_threading(design); 
    mcf_multi_threading.run(movements);
    
    std::cout << "movements after ILP " << movements.size() << std::endl;
    
    auto end_time = std::chrono::steady_clock::now();

    std::chrono::duration<double> diff = end_time-start_time;
    auto current_time = diff.count() / 60.0;
    auto remaining_time = 55 - current_time;

    std::cout << "current time " << current_time << " remaining time " << remaining_time << std::endl;
    
    for (auto movement : movements) {
        auto cell = movement.first;
        design.placement().fixLocation(cell);
    }

    ophidian::routing::AStarRouting astar_routing{design};
    auto cell_costs = compute_cell_move_costs_descending_order(design);
    
    /*auto debug_gcell = design.global_routing().gcell_graph()->gcell(23, 10, 0);
    auto capacity = design.global_routing().gcell_graph()->capacity(debug_gcell);
    auto demand = design.global_routing().gcell_graph()->demand(debug_gcell);
    auto layer_index = design.global_routing().gcell_graph()->layer_index(debug_gcell);
    auto gcell_box = design.global_routing().gcell_graph()->box(debug_gcell);

    std::cout << "debug gcell before moves " << gcell_box.min_corner().y().value() << " " << gcell_box.min_corner().x().value() << " " << layer_index << std::endl;
    std::cout << "debug gcell before moves capacity " << capacity << " demand " << demand << std::endl;*/

    move_cells_for_until_x_minutes(design, remaining_time, cell_costs, movements, astar_routing);

    ophidian::parser::ICCAD2020Writer iccad_output_writer(design, circuit_name);

    iccad_output_writer.write_ICCAD_2020_output(output, movements);

    log() << "end" << std::endl;
}//end run_mcf_for_circuit

void run_for_circuit(ophidian::design::Design & design, std::string circuit_name, std::string output) {
    auto & netlist = design.netlist();
    auto& global_routing = design.global_routing();
    ophidian::routing::AStarRouting astar_routing{design};
    //ophidian::routing::ILPRouting ilpRouting(design, circuit_name);
    ophidian::parser::ICCAD2020Writer iccad_output_writer(design, circuit_name);

    std::vector<ophidian::circuit::Net> nets(design.netlist().begin_net(), design.netlist().end_net());
    std::vector<ophidian::circuit::Net> fixed_nets;
    std::vector<ophidian::circuit::Net> routed_nets;

    //std::vector<std::pair<ophidian::routing::ILPRouting::cell_type, ophidian::routing::ILPRouting::point_type>> movements;
    // std::log() << "routing nets" << std::endl;
    //auto result = ilpRouting.route_nets(nets, fixed_nets, routed_nets, movements);
    // std::log() << "result " << result << std::endl;


    for(auto net_it = netlist.begin_net(); net_it != netlist.end_net(); net_it++)
    {
        global_routing.unroute(*net_it);
        std::vector<ophidian::routing::AStarSegment> segments;
        astar_routing.route_net(*net_it, segments);
    }


    // auto cells = compute_cell_move_costs_descending_order(design);
    // move_cells_for_until_x_minutes(design, 40, cells, movements, astar_routing);

    
    
    // for(auto net_it = netlist.begin_net(); net_it != netlist.end_net(); net_it++)
    // {
    //     global_routing.unroute(*net_it);
    //     std::vector<ophidian::routing::AStarSegment> segments;
    //     astar_routing.route_net(*net_it, segments);
    // }





    iccad_output_writer.write_ICCAD_2020_output(output, {});
    // if(result.first){
    //     iccad_output_writer.write_ICCAD_2020_output(output, movements);
    // }

    // std::log() << "connected nets" << std::endl;
    // for (auto net : nets) {
    //     ophidian::routing::GlobalRouting::gcell_container_type pin_gcells = {};
    //     for (auto pin : design.netlist().pins(net)) {
    //         auto pin_name = design.netlist().name(pin);                
    //         auto location = design.placement().location(pin);
    //         auto box = ophidian::routing::GCellGraph::box_type{location, location};
    //         auto pin_geometry = design.placement().geometry(pin);
    //         auto layer_name = pin_geometry.front().second;
    //         auto pin_layer = design.routing_library().find_layer_instance(layer_name);
    //         auto layer_index = design.routing_library().layerIndex(pin_layer);

    //         // std::log() << "pin " << pin_name << " layer " << layer_name << " index " << layer_index << std::endl;

    //         design.global_routing().gcell_graph()->intersect(pin_gcells, box, layer_index-1);
    //     }
    //     auto connected = design.global_routing().is_connected(net, pin_gcells);

    //     auto net_name = design.netlist().name(net);
    //     if(!connected)
    //         std::log() << "net " << net_name << " is open" << std::endl;
    // }
}

bool test_input(const std::string input_file)
{
    if(input_file.find(".txt") != std::string::npos)
        return true;
    return false;
}

std::string extract_circuit_name(const std::string input_file)
{
    // Find the position of first delimiter ( last / )
    int firstDelPos = input_file.rfind("/");
    // Find the position of second delimiter
    int secondDelPos = input_file.rfind(".txt");
    auto circuit = input_file.substr(firstDelPos + 1, secondDelPos - firstDelPos - 1);
    return circuit;
}

int main(int argc, char** argv) {
    //start_time = std::chrono::steady_clock::now();

    greetings();

    bool input_found{false};
    string input_file{};
    string circuit_name{};

    bool output_found{false};
    string output{};

    // HELP
    if (argc == 1 || string(argv[1]) == "-h" || string(argv[1]) == "-help" || string(argv[1]) == "--help")
    {
        log() << "usage:" << endl;
        log() << "./cell_move_router <input.txt> <output.txt>" << endl;
        return 0;
    }

    if (argc < 2)
    {
        log() << "Cannot proceed, missing inputn and/or output file name" << endl;
        log() << "For help, set --help or -help or -h" << endl;
        log() << "usage: ./cell_move_router <input.txt> <output.txt>" << endl;
        return 0;
    } 

    if (argv[1])
    {
        input_file = argv[1];
        if(test_input(input_file))
        {
            input_found = true;
            circuit_name = extract_circuit_name(input_file);
        }
    }

    if (argv[2])
    {
        output = argv[2];
        if(test_input(output))
        {
            output_found = true;
        }
    }

    // must have flags:
    if (input_file == "")
    {
        log() << "Cannot proceed, missing Input file" << endl;
        log() << "For help, set --help or -help or -h" << endl;
        return 0;
    }

    if (output == "")
    {
        log() << "Cannot proceed, missing output file name" << endl;
        log() << "For help, set --help or -help or -h" << endl;
        return 0;
    }

    auto iccad_2020 = ophidian::parser::ICCAD2020{input_file};

    auto design = ophidian::design::Design();
    ophidian::design::factory::make_design_iccad2020(design, iccad_2020);
    
    //run_for_circuit(design, circuit_name, output);
    run_mcf_for_circuit(design,circuit_name, output);

    return 0;
}
