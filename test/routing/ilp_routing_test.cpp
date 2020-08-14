#include <catch.hpp>

#include <ophidian/design/Design.h>
#include <ophidian/design/DesignFactory.h>
#include <ophidian/parser/ICCAD2020.h>
#include <ophidian/routing/ILPRouting.h>

TEST_CASE("Test for iccad2020 ilp routing.", "[routing][globalRouting][ilp]")
{
    ophidian::parser::ICCAD2020 sample{"input_files/iccad2020/cases/case1.txt"};
    //ophidian::parser::ICCAD2020 sample{"input_files/iccad2020/cases/case3_no_extra_demand.txt"};
    auto design = ophidian::design::Design{};
    ophidian::design::factory::make_design_iccad2020(design, sample);
    ophidian::routing::ILPRouting<IloBoolVar> ilpRouting(design, "case1");

    //ophidian::routing::ILPRouting ilpRouting(design, "case3_no_extra_demand");

    std::vector<ophidian::circuit::Net> nets(design.netlist().begin_net(), design.netlist().end_net());
    std::vector<ophidian::circuit::CellInstance> cells(design.netlist().begin_cell_instance(), design.netlist().end_cell_instance());
    using box_type = ophidian::placement::Placement::box_type;
    auto area = box_type{design.floorplan().chip_origin(), design.floorplan().chip_upper_right_corner()};

    std::vector<ophidian::circuit::Net> fixed_nets;
    std::vector<ophidian::circuit::Net> routed_nets;
    std::vector<ophidian::circuit::Net> unrouted_nets;
    
    std::vector<std::pair<ophidian::routing::ILPRouting<IloBoolVar>::cell_type, ophidian::routing::ILPRouting<IloBoolVar>::point_type>> movements; 
    std::cout << "routing nets" << std::endl;
    auto result = ilpRouting.route_nets(nets, cells, area, fixed_nets, routed_nets, unrouted_nets, movements);
    std::cout << "result " << result.first << std::endl;
    REQUIRE(result.first);
}
