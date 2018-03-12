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

#ifndef VERILOGPARSER_H
#define VERILOGPARSER_H

#include <memory>
#include <istream>
#include <list>
#include <map>

namespace ophidian
{
    namespace parser
    {
        class Verilog
        {
        public:
            enum class PortDirection {
                INPUT, OUTPUT, INOUT, NONE
            };

            class Port
            {
            public:

                Port(PortDirection direction, const std::string & name);

                PortDirection direction() const;

                const std::string & name() const;

                bool operator==(const Port & o) const;

            private:
                PortDirection mDirection;
                std::string   mName;
            };

            class Net
            {
            public:

                Net(const std::string & name);

                const std::string & name() const;

                bool operator==(const Net & o) const;

            private:
                std::string mName;
            };

            class Module;
            class Instance
            {
            public:

                Instance(Module * module, const std::string name);

                Module * module() const;

                const std::string & name() const;

                void mapPort(const Port * port, const Net * net);

                const std::map <const Port *, const Net *> & portMapping() const;

            private:
                Module *                             mModule;
                std::string                          mName;
                std::map <const Port *, const Net *> mPortMapping;
            };

            class Module
            {
            public:

                Module(const std::string & name);

                Port * addPort(Verilog::PortDirection direction, const std::string name);

                Net * addNet(const std::string & name);

                Module * addModule(const std::string & name);

                Instance * addInstance(Module * module, const std::string & name);

                const std::string & name() const;

                const std::list <Port> & ports() const;

                const std::list <Net> & nets() const;

                const std::list <Module> & modules() const;

                const std::list <Instance> & instances() const;

            private:
                std::string          mName;
                std::list <Port>     mPorts;
                std::list <Net>      mNets;
                std::list <Module>   mModules;
                std::list <Instance> mInstances;
            };

            Module * addModule(const std::string & name);

            const std::list <Module> & modules() const;

        private:
            std::list <Module> mModules;
        };

        class VerilogParser
        {
        public:

            VerilogParser();

            ~VerilogParser();

            Verilog * readStream(std::istream & in);

            Verilog * readFile(const std::string & filename);

        private:
            struct Impl;

            std::unique_ptr <Impl> mThis;
        };
    }     // namespace parser
}     // namespace ophidian

#endif // VERILOGPARSER_H