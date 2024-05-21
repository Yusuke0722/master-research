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

package delegate_selection_on_bft;

import inet.node.ethernet.EthernetLink;
import inet.node.ethernet.Eth100M;
import inet.node.ethernet.Eth10G;

//
// TODO auto-generated module
//
network Delegate_selection_on_BFT
{
    types:
        channel Lpwa extends EthernetLink {
            datarate = 50kbps;
        }
        simple Create
        {
        	parameters:
        		@display("i=block/routing");
            gates:
                inout gate[];
        }

    submodules:
        Creator[5]: Create;
        Adversary[2]: Create {
        	parameters:
        		@display("i=,red");
        }
    connections:
        for i=0..sizeof(Creator)-1, for j=0..sizeof(Creator)-1 {
            Creator[i].gate++ <--> Lpwa <--> Creator[j].gate++ if i<j;
            //Creator[i].gate++ <--> Eth100M <--> Creator[j].gate++ if i<j;
            //Creator[i].gate++ <--> Eth1G <--> Creator[j].gate++ if i<j;
            //Creator[i].gate++ <--> Creator[j].gate++ if i<j;
        }
        for i=0..sizeof(Adversary)-1, for j=0..sizeof(Adversary)-1 {
            Adversary[i].gate++ <--> Lpwa <--> Adversary[j].gate++ if i<j;
            //Creator[i].gate++ <--> Adversary[j].gate++;
        }
        for i=0..sizeof(Creator)-1, for j=0..sizeof(Adversary)-1 {
            Creator[i].gate++ <--> Lpwa <--> Adversary[j].gate++;
            //Creator[i].gate++ <--> Adversary[j].gate++;
        }
}
