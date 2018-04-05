/*
 * Copyright 2017 Ophidian
   Licensed to the Apache Software Foundation (ASF) under one
   or more contributor license agreements.  See the NOTICE file
   distributed with this work for additional information
   regarding copyright ownership.  The ASF licenses this file
   to you under the Apache License, Version 2.0 (the
   "License"); you may not use this file except in compliance
   with the License.  You may obtain a copy of the License at
   http://www.apache.org/licenses/LICENSE-2.0
   Unless required by applicable law or agreed to in writing,
   software distributed under the License is distributed on an
   "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
   KIND, either express or implied.  See the License for the
   specific language governing permissions and limitations
   under the License.
 */

#include "Placement.h"

namespace ophidian
{
namespace placement
{

Placement::Placement(const circuit::Netlist &netlist) :
    mCellLocations(netlist.makeProperty<util::LocationDbu>(circuit::Cell())),
    mInputLocations(netlist.makeProperty<util::LocationDbu>(circuit::Input())),
    mOutputLocations(netlist.makeProperty<util::LocationDbu>(circuit::Output())),
    mCellFixed(netlist.makeProperty<bool>(circuit::Cell()))
{
}

Placement::~Placement()
{
}

void Placement::placeCell(const circuit::Cell &cell, const util::LocationDbu &location)
{
    mCellLocations[cell] = location;
}

void Placement::placeInputPad(const circuit::Input &input, const util::LocationDbu &location)
{
    mInputLocations[input] = location;
}

void Placement::fixLocation(const circuit::Cell &cell, bool fixed)
{
    mCellFixed[cell] = fixed;
}

bool Placement::isFixed(const circuit::Cell &cell) const
{
    return mCellFixed[cell];
}

util::LocationDbu Placement::inputPadLocation(const circuit::Input &input) const
{
    return mInputLocations[input];
}

void Placement::placeOutputPad(const circuit::Output &output, const util::LocationDbu &location)
{
    mOutputLocations[output] = location;
}

util::LocationDbu Placement::outputPadLocation(const circuit::Output &output) const
{
    return mOutputLocations[output];
}

} //namespace placement
} //namespace ophidian