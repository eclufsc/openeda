#include <catch.hpp>

#include <ophidian/parser/Lef.h>
#include <iostream>

using namespace ophidian;

bool compare(const parser::Lef::site & a, const parser::Lef::site & b)
{
	return a.name == b.name &&
			a.mClass == b.mClass &&
			a.symmetry == b.symmetry &&
			Approx(a.x) == b.x &&
			Approx(a.y) == b.y;
}

bool compare(const parser::Lef::layer & a, const parser::Lef::layer & b)
{
	return a.name == b.name &&
			a.type == b.type &&
			a.direction == b.direction &&
			Approx(a.pitch) == b.pitch &&
			Approx(a.width) == b.width;
}

bool compare(const parser::Lef::rect & a, const parser::Lef::rect & b)
{
	return Approx(units::unit_cast<double>(a.firstPoint.x())) == units::unit_cast<double>(b.firstPoint.x()) &&
			Approx(units::unit_cast<double>(a.firstPoint.y())) == units::unit_cast<double>(b.firstPoint.y()) &&
			Approx(units::unit_cast<double>(a.secondPoint.x())) == units::unit_cast<double>(b.secondPoint.x()) &&
			Approx(units::unit_cast<double>(a.secondPoint.y())) == units::unit_cast<double>(b.secondPoint.y());
}

bool compare(const std::vector<parser::Lef::rect> & a, const std::vector<parser::Lef::rect> & b)
{
	bool rectsAreEqual = true;

	for (auto& i : a)
	{
		auto comparePredicate = [i](const parser::Lef::rect & layer) -> bool {
			return compare(i, layer);
		};

		rectsAreEqual = rectsAreEqual && std::find_if(a.begin(), a.end(), comparePredicate) != a.end();
	}

	return rectsAreEqual;
}

bool compare(const std::vector<std::string> & a, const std::vector<std::string> & b)
{
	bool layersAreEqual = true;

	for (auto& i : a)
	{
		auto comparePredicate = [i](const std::string & layer) -> bool {
			return i == layer;
		};

		layersAreEqual = layersAreEqual && std::find_if(a.begin(), a.end(), comparePredicate) != a.end();
	}

	return layersAreEqual;
}

bool compare(const parser::Lef::port & a, const parser::Lef::port & b)
{
	return compare(a.layers, b.layers) &&
			compare(a.rects, b.rects);
}

bool compare(const std::vector<parser::Lef::port> & a, const std::vector<parser::Lef::port> & b)
{
	bool portsAreEqual = true;

	for (auto& i : a)
	{
		auto comparePredicate = [i](const parser::Lef::port & port) -> bool {
			return compare(i, port);
		};

		portsAreEqual = portsAreEqual && std::find_if(a.begin(), a.end(), comparePredicate) != a.end();
	}

	return portsAreEqual;
}

bool compare(const parser::Lef::pin & a, const parser::Lef::pin & b)
{
	return a.name == b.name &&
			a.direction == b.direction &&
			compare(a.ports, b.ports);
}

bool compare(const parser::Lef::macro_size & a, const parser::Lef::macro_size & b)
{
	return Approx(a.x) == b.x &&
			Approx(a.y) == b.y;
}

bool compare(const parser::Lef::macro_foreign & a, const parser::Lef::macro_foreign & b)
{
	return a.name == b.name &&
			Approx(a.x) == b.x &&
			Approx(a.y) == b.y;
}

bool compare(const std::vector<parser::Lef::pin> & a, const std::vector<parser::Lef::pin> & b)
{
	bool pinsAreEqual = true;

	for (auto& i : a)
	{
		auto comparePredicate = [i](const parser::Lef::pin & pin) -> bool {
			return compare(i, pin);
		};

		pinsAreEqual = pinsAreEqual && std::find_if(a.begin(), a.end(), comparePredicate) != a.end();
	}

	return pinsAreEqual;
}

bool compare(const parser::Lef::macro & a, const parser::Lef::macro & b)
{
	return a.name == b.name &&
			a.mClass == b.mClass &&
			compare(a.pins, b.pins) &&
			compare(a.foreign, b.foreign) &&
			compare(a.size, b.size) &&
			a.site == b.site &&
			compare(a.origin, b.origin);
}

bool compare(const parser::Lef::obs & a, const parser::Lef::obs & b)
{
	auto pred = [] (auto lhs, auto rhs)
	{
		return lhs.first == rhs.first;
	};

	auto aMap = a.layer2rects;
	auto bMap = b.layer2rects;

	return aMap.size() == bMap.size()
			&& std::equal(aMap.begin(), aMap.end(), bMap.begin(), bMap.end(), pred);
}

TEST_CASE("lef: missing file", "[parser][lef][missing_file]")
{
	parser::LefParser parser;
	std::unique_ptr<ophidian::parser::Lef> lef =  std::make_unique<ophidian::parser::Lef>();
	REQUIRE_THROWS(parser.readFile("thisFileDoesNotExist.lef", lef));
}

TEST_CASE("lef: simple.lef parsing", "[parser][lef][simple]")
{
	parser::LefParser parser;

	std::unique_ptr<parser::Lef> simpleLef = std::make_unique<ophidian::parser::Lef>();
	parser.readFile("input_files/simple/simple.lef", simpleLef);

	SECTION("Sites are parsed correctly", "[parser][lef][simple]")
	{
		CHECK( simpleLef->sites().size() == 1 );

		parser::Lef::site core;
		core.name = "core";
		core.mClass = "CORE";
		core.x = 0.19;
		core.y = 1.71;

		REQUIRE(compare(simpleLef->sites().front(), core));
	}

	SECTION("Layers are parsed correctly", "[parser][lef][simple][layers]")
	{
		std::vector<parser::Lef::layer> layers {
			{"metal1", "ROUTING", parser::Lef::layer::HORIZONTAL,  0.2, 0.1},
			{"metal2", "ROUTING", parser::Lef::layer::VERTICAL,    0.2, 0.1},
			{"metal3", "ROUTING", parser::Lef::layer::HORIZONTAL,  0.2, 0.1}
		};

		CHECK( simpleLef->layers().size() == layers.size() );

		for(auto & simple_layer : layers)
		{
			auto comparePredicate = [simple_layer](const parser::Lef::layer & layer) -> bool {
				return compare(simple_layer, layer);
			};

			REQUIRE( std::find_if(
						 simpleLef->layers().begin(),
						 simpleLef->layers().end(),
						 comparePredicate
						 ) != simpleLef->layers().end());
		}
	}

	SECTION("Macros are parsed correctly", "[parser][lef][simple][macros]")
	{
		CHECK( simpleLef->macros().size() == 212 );

		std::vector<std::string> m1_pin_layers = {"metal1"};

		std::vector<parser::Lef::rect> m1_o_rects = {
			{util::LocationMicron(0.465, 0.150), util::LocationMicron(0.53, 1.255)},
			{util::LocationMicron(0.415, 0.150), util::LocationMicron(0.61, 0.28)}
		};

		std::vector<parser::Lef::rect> m1_a_rects = {
			{util::LocationMicron(0.210, 0.340), util::LocationMicron(0.34, 0.405)}
		};

		parser::Lef::port m1_o_port = {m1_pin_layers, m1_o_rects};
		parser::Lef::port m1_a_port = {m1_pin_layers, m1_a_rects};

		parser::Lef::pin o = {"o", parser::Lef::pin::OUTPUT, {m1_o_port}};
		parser::Lef::pin a = {"a", parser::Lef::pin::INPUT, {m1_a_port}};

		parser::Lef::macro_foreign m1_foreign = {"INV_X1", 0.000, 0.000};

		parser::Lef::macro m1;
		m1.name = "INV_X1";
		m1.mClass = "CORE";
		m1.pins = {o, a};
		m1.foreign = m1_foreign;
		m1.size = {0.760, 1.71};
		m1.site = "core";
		m1.origin = {0.000, 0.000};

		REQUIRE(compare(simpleLef->macros().front(), m1));
	}

	SECTION("Database units are correct", "[parser][lef][simple][dbunits]")
	{
		CHECK(Approx(simpleLef->databaseUnits()) == 2000.0);
	}
}
