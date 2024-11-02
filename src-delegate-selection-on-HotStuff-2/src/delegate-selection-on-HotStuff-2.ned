//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

package delegate_selection_on_hotstuff_2;

import inet.common.misc.ThruputMeteringChannel;

network Delegate_selection_on_HotStuff_2
{
    parameters:
        int createNum = 1000;
        int concNum = 10;
    types:
        channel lteup extends ThruputMeteringChannel
        {
            datarate = 5Mbps;
            delay =300ms;
        }
        channel ltedown extends ThruputMeteringChannel
        {
            datarate = 5Mbps;
            delay = 300ms;
        }
        channel optic extends ThruputMeteringChannel
        {
            datarate = 200Mbps;
            delay = 30ms;
        }
        simple Create
        {
            parameters:
                @display("i=misc/house");
            gates:
                input in[];
                output out[];
        }
        simple Concentrate
        {
            parameters:
                @display("i=block/routing");
            gates:
                input in[];
                output out[];
                inout gate[];
        }
    submodules:
        Creator[createNum]: Create;
        Concentrator[concNum]: Concentrate;
    connections:
        for i=0..sizeof(Creator)-1 {
            Creator[i].out++ --> lteup --> Concentrator[int(i*concNum/createNum)].in++;
            Concentrator[int(i*concNum/createNum)].out++ --> ltedown --> Creator[i].in++;
        }
        for i=0..sizeof(Concentrator)-1, for j=0..sizeof(Concentrator)-1 {
            Concentrator[i].gate++ <--> optic <--> Concentrator[j].gate++ if i<j;
        }
}
