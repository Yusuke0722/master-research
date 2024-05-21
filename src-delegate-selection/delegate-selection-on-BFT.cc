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

#include <string>
#include <vector>
#include <stdio.h>
#include <omnetpp.h>
#include <nlohmann/json.hpp>
#include "delegate-selection-on-BFT.h"

#define BUFFER_MAX 4096
#define LLAMBDA 60.0
#define SLAMBDA 10.0
#define PROP_NUM 5
#define VER_NUM 10
#define STEP_MAX 3


using namespace omnetpp;
using namespace std;
using json = nlohmann::json;

struct block {
    uint32_t contents_length;
    uint32_t step;
    int contents_hash;
    int previous_hash;
    int sign;
};

class Create: public cSimpleModule {
private:
    cHistogram createTime;
    simtime_t addingTime;
    simtime_t stepTime;
    block newBlock;
    uint32_t round;
    uint32_t next_round;
    uint32_t chainLen;
    uint32_t messageNum;
    uint32_t ownedToken;
    uint32_t wholeToken;
    int gate_size;
    int tL;
    int tH;
    float scale;
    int lead_sign;
    vector<json> messages;
protected:
    void initialize() override;
    void send_to_all(const string name);
    void handleMessage(cMessage *msg) override;
    void publish_message(uint32_t step, int sign);
    void step12();
    void find_leader(uint32_t step);
    void mineBlock();
    bool is_leader(json message, vector<json> value, float num);
    void find_value(uint32_t step);
    void next_step();
    void clean_message();
    void finish_round();
    void finish() override;
};

//Define_Module(Delegate_selection_on_BFT);
Define_Module(Create);

void Create::initialize() {
    chainLen = 0;
    messageNum = 0;
    gate_size = gateSize("gate");
    ownedToken = getId() - 1;
    wholeToken = 0;
    for (int i = 1; i <= gate_size + 1; i++) { wholeToken += i; }
    // 1 <= scale <= 10
    //scale = 10;
    //tH = (gate_size < VER_NUM) ? 0.69*gate_size*scale : 0.69*VER_NUM*scale;
    tL = 1;
    tH = ((gate_size + 1) / 3) * 2 + 1;  // without itself
    //tH = 3;
    EV << "tH: " << tH << endl;
    //tH = (gate_size < VER_NUM) ? (gate_size/3)*2 : (VER_NUM/3)*2;
    addingTime = simTime();
    round = 0;
    next_round = 0;
    srand(getId());
    scheduleAt(simTime(), new cMessage("create"));
}

void Create::send_to_all(const string name) {
    for (int i = 0; i < gate_size; i++) {
        cPacket *msg = new cPacket(name.c_str());
        msg->setByteLength(200);
        //EV << "size: " << msg->getByteLength() << endl;
        sendDelayed(msg, SLAMBDA, "gate$o", i);
        /*
        simtime_t endTrsm = gate("gate$o", i)->getTransmissionChannel()->getTransmissionFinishTime();
        if (endTrsm < simTime()) {
            send(msg, "gate$o", i);
        } else {
            sendDelayed(msg, endTrsm - simTime(), "gate$o", i);
        }
        */
    }
}

void Create::handleMessage(cMessage *msg) {
    if (!(msg->isSelfMessage())) { messageNum++; }
    string str = string(msg->getName());
    if (str == "create") {
        mineBlock();
        delete msg;
        return;
    }
    json m = json::parse(str);
    messages.push_back(m);
    int step = int(m["step"]) + 1;
    if (step == newBlock.step) {
        if (newBlock.step == 2) {
            find_leader(step);
        } else {
            find_value(step);
        }
    }
    delete msg;
}

block buildBlock(const block *previous, const char *contents, uint64_t length) {
    block header;
    header.contents_length = length;
    if (previous) {
        /* calculate previous block header hash */
        header.previous_hash = previous->contents_hash;
    } else {
        /* genesis has no previous. just use zeroed hash */
        header.previous_hash = 0;
    }
    /* add data hash */
    header.contents_hash = rand();
    return header;
}

void Create::publish_message(uint32_t step, int sign) {
    if (step > STEP_MAX) { return; }
    if (getId() > gate_size + 1 - ((gate_size + 1) / 3) + 1) { return; }
    if (step == 2 && newBlock.sign == lead_sign) { return; }
    int num = (step == 1) ? PROP_NUM : VER_NUM;
    //float dif = 256 * num * ownedToken * scale / wholeToken;
    float dif = 256 * num / (gate_size + 1);
    uint8_t selected[32];
    //calc_sha_256(selected, &newBlock, sizeof(block));
    int d = sign % (gate_size + 1);
    int k = (d + 4) % (gate_size + 1);
    EV << d << k << endl;
    d += 2;
    k += 2;
    if (getId() == round % (gate_size + 1) + 2 || step != 1) {
    //if (true) {
    //if (static_cast<uint32_t>(selected[0]) < dif) {
        json j = {{"round", round}, {"step", step}, {"sign", sign}};
        if (step == 1 || d <= getId() && getId() < k || k < d && (getId() < k || d <= getId())) {
            messages.push_back(j);
            send_to_all(j.dump());
        }
        if (step == 1) {
            EV << "proposing block" << endl;
            lead_sign = sign;
            next_step();
        }
    }
}

void Create::find_leader(uint32_t step) {
    int count = 0;
    for (json m : messages) {
        if (m["round"] == round && m["step"] == 1) {
            count++;
            if (m["sign"] < lead_sign) {
                lead_sign = m["sign"];
            }
        }
    }
    if (count >= tL || simTime() - stepTime >= 2*SLAMBDA) {
        publish_message(2, lead_sign);
        next_step();
    }
}

void Create::step12() {
    newBlock.step = 1;
    publish_message(1, newBlock.sign);
    EV << "step2" << endl;
    newBlock.step++;
    lead_sign = RAND_MAX;
    stepTime = simTime();
    json j = {{"step", newBlock.step - 1}};
    //scheduleAt(simTime() + 2*SLAMBDA, new cMessage(j.dump().c_str()));
    scheduleAt(simTime() + LLAMBDA, new cMessage(j.dump().c_str()));
}

void Create::mineBlock() {
    next_round = round + 1;
    char line_buffer[BUFFER_MAX] = "first";
    uint64_t size = strnlen(line_buffer, BUFFER_MAX) + 1;

    block *previous_ptr = NULL;
    newBlock = buildBlock(previous_ptr, line_buffer, size);
    newBlock.sign = rand();
    step12();
}

bool Create::is_leader(json message, vector<json> value, float num) {
    int count = 1;
    if (message["step"] == 2 && newBlock.sign == lead_sign) { count++; }
    for (json v : value) {
        if (message["sign"] == v["sign"] && message["step"] == v["step"]) { count++; }
        if (count >= num) { return true; }
    }
    //EV << "ID: " << getId() << ", count: " << count << endl;
    return false;
}

void Create::find_value(uint32_t step) {
    vector<json> value;
    for (json m : messages) {
        if (m["round"] == round && m["step"] == step - 1) {
            value.push_back(m);
            if (is_leader(m, value, tH)) {
                lead_sign = m["sign"];
                EV << "ID: " << getId() << ", sign" << lead_sign << endl;
                publish_message(step, lead_sign);
                next_step();
                return;
            }
        }
    }
}

void Create::next_step() {
    if (newBlock.step > STEP_MAX) {
        EV << "verified!" << endl;
        finish_round();
        return;
    }
    newBlock.step++;
    EV << "ID: " << getId() << ", step" << newBlock.step << endl;
    json j = {{"step", newBlock.step - 1}};
    stepTime = simTime();
    find_value(newBlock.step);
    scheduleAt(simTime() + LLAMBDA, new cMessage(j.dump().c_str()));
}

void Create::clean_message() {
    for (auto itr = messages.begin(); itr != messages.end();) {
        if ((*itr)["round"] < round) {
            itr = messages.erase(itr);
        } else {
            itr++;
        }
    }
}

void Create::finish_round() {
    if (round == next_round) { return; }
    createTime.collect(simTime() - addingTime);
    addingTime = simTime();
    chainLen++;
    round = next_round;
    clean_message();
    mineBlock();
}

void Create::finish() {
    EV << "Total blocks Count:            " << chainLen << endl;
    EV << "Total messages Count:          " << messageNum << endl;
    EV << "Total jobs Count:              " << createTime.getCount() << endl;
    EV << "Total jobs Min createtime:     " << createTime.getMin() << endl;
    EV << "Total jobs Mean createtime:    " << createTime.getMean() << endl;
    EV << "Total jobs Max createtime:     " << createTime.getMax() << endl;
    EV << "Total jobs Standard deviation: " << createTime.getStddev() << endl;

    createTime.recordAs("create time");
}
