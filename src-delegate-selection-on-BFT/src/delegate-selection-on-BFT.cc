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
#define LLAMBDA 20
#define SLAMBDA 5
#define WAIT_MAX 10
#define BLOCK_MAX 2*8192 * 1000
#define TX_SIZE 512
#define TX_SPEED 1000
#define STEP_MAX 3
#define CREATE_NUM 1000
#define CONC_NUM 10


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
    cHistogram throughput;
    simtime_t addingTime;
    simtime_t waitingTime;
    block newBlock;
    uint32_t round;
    uint32_t next_round;
    uint32_t chainLen;
    uint32_t blockSize;
    uint32_t lstBlockSize;
    uint32_t messageNum;
    uint32_t ownedToken;
    uint32_t wholeToken;
    int gate_size;
    int nD;
    int nL;
    int tH;
    int waitingSize;
    int lead_sign;
    int msgSize;
    bool isBot;
    //int fault[10] = {4,15,17,22,31,44,45,53,56,77};
    //int fault[25] = {2,7,9,10,21,23,25,29,30,31,38,51,53,57,61,62,64,65,66,69,71,76,96,97,99};
    int fault[100] = {781,233,244,915,647,382,541,958,0,981,903,357,844,912,297,823,713,763,166,867,840,936,27,345,340,437,470,830,570,214,181,315,687,131,36,544,398,325,858,622,128,976,681,212,971,749,7,686,762,946,787,740,246,425,278,707,424,985,431,117,798,252,531,279,313,119,594,723,104,205,987,634,935,873,106,864,826,369,399,448,261,430,84,874,708,534,43,239,659,615,848,689,391,655,364,445,22,349,918,834};
    vector<json> messages;
    cPacketQueue packetQueue;
protected:
    void initialize() override;
    void send_to_all(const int step, const int sign);
    void send_to_id(const int step, const int sign, const int fromId, const int toId);
    void handleMessage(cMessage *msg) override;
    bool isCommittee(int id, int fromId, int toId);
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

class Concentrate: public cSimpleModule {
protected:
    void handleMessage(cMessage *msg) override;
};

Define_Module(Create);
Define_Module(Concentrate);

void Create::initialize() {
    chainLen = 0;
    blockSize = 0;
    lstBlockSize = 0;
    messageNum = 0;
    gate_size = gateSize("out");
    nD = 22;
    tH = (nD / 3) * 2 + 1;
    nL = 3;
    //EV << "tH: " << tH << endl;
    addingTime = simTime();
    waitingTime = simTime();
    waitingSize = 0;
    round = 0;
    next_round = 0;
    scheduleAt(simTime() + 100, new cMessage("record"));
    scheduleAt(simTime(), new cMessage("create"));
}

void Create::send_to_all(const int step, const int sign) {
    json j = {{"round", round}, {"step", step}, {"sign", sign}};
    messages.push_back(j);
    string name = j.dump();
    for (int i = 0; i < CREATE_NUM - 1; i++) {
        cPacket *msg = new cPacket(name.c_str());
        if (step == 1) {
            msg->setByteLength(msgSize);
        } else {
            msg->setByteLength(64);
        }
        //EV << "size: " << msg->getByteLength() << endl;
        double leftTime = (WAIT_MAX - (simTime() - waitingTime)).dbl();
        //simtime_t endTrsm = gate("out", i)->getTransmissionChannel()->getTransmissionFinishTime();
        if (newBlock.step != 1 || waitingSize > BLOCK_MAX) {
            send(msg, "out", i);
        } else if ((double)(BLOCK_MAX - waitingSize) / (TX_SIZE * TX_SPEED) < leftTime) {
            sendDelayed(msg, (double)(BLOCK_MAX - waitingSize) / (TX_SIZE * TX_SPEED), "out", i);
        } else {
            sendDelayed(msg, WAIT_MAX - (simTime() - waitingTime), "out", i);
        }
    }
}

void Create::send_to_id(const int step, const int sign, const int fromId, const int toId) {
    vector<int> committee;
    int endId = (fromId < toId) ? toId : toId + CREATE_NUM;
    for (int i = fromId; i < endId; i++) {
        if (i != getId() - 2) {
            committee.push_back(i % CREATE_NUM);
        }
    }
    json j = {{"round", round}, {"step", step}, {"sign", sign}, {"sender", getId() - 2}};
    messages.push_back(j);
    j["committee"] = committee;
    cPacket *msg = new cPacket(j.dump().c_str());
    if (step == 1) {
        msg->setByteLength(msgSize);
    } else {
        msg->setByteLength(64);
    }
    //EV << "size: " << msg->getByteLength() << endl;
    double leftTime = (WAIT_MAX - (simTime() - waitingTime)).dbl();
    simtime_t endTrsm = gate("out", 0)->getTransmissionChannel()->getTransmissionFinishTime();
    if (newBlock.step != 1 || waitingSize > BLOCK_MAX) {
        sendDelayed(msg, max((endTrsm - simTime()).dbl(), 0.0), "out", 0);
    } else if ((double)(BLOCK_MAX - waitingSize) / (TX_SIZE * TX_SPEED) < leftTime) {
        sendDelayed(msg, (double)(BLOCK_MAX - waitingSize) / (TX_SIZE * TX_SPEED) + max((endTrsm - simTime()).dbl(), 0.0), "out", 0);
    } else {
        sendDelayed(msg, max(leftTime, 0.0) + max((endTrsm - simTime()).dbl(), 0.0), "out", 0);
    }
}

void Create::handleMessage(cMessage *msg) {
    if (!(msg->isSelfMessage())) { messageNum++; }
    string str = string(msg->getName());
    if (str == "record") {
        throughput.collect((double)(blockSize - lstBlockSize) / TX_SIZE / 100);
        lstBlockSize = blockSize;
        scheduleAt(simTime() + 100, new cMessage("record"));
        delete msg;
        return;
    } else if (str == "create") {
        mineBlock();
        delete msg;
        return;
    }
    json m = json::parse(str);
    if (m["round"] < round || m["sign"] == nullptr) {
        if (m["round"] == round && simTime() - addingTime < LLAMBDA && messages.size() > 0) {
            find_leader(newBlock.step);
        } else if (m["round"] == round && simTime() - addingTime >= LLAMBDA) {
            isBot = true;
            finish_round();
        }
    } else {
        messages.push_back(m);
    }
    int step = int(m["step"]) + 1;
    if (m["round"] == round && step >= newBlock.step && m["sign"] != nullptr) {
        if (step == 2) {
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

bool Create::isCommittee(int id, int fromId, int toId) {
    return (fromId <= id && id < toId || toId < fromId && (id < toId || fromId <= id) || fromId == toId);
}

void Create::publish_message(uint32_t step, int sign) {
    if (step > STEP_MAX) { return; }
    // Corrupt nodes' behavior
    if (count(begin(fault), end(fault), getId() - 2) > 0) { return; }
    //if (getId() > CREATE_NUM - (CREATE_NUM / 3) + 1) { return; }
    //if (step == 2 && newBlock.sign == lead_sign) { return; }
    int fromL = sign % CREATE_NUM;
    int toL = (fromL + nL) % CREATE_NUM;
    int fromC = (fromL * round) % CREATE_NUM;
    int toC = (fromC + nD) % CREATE_NUM;
    int fromD = (fromC * round) % CREATE_NUM;
    int toD = (fromD + nD) % CREATE_NUM;
    EV << fromL << " " << toL << " " << fromC << " " << toC << " " << fromD << " " << toD << endl;
    double leftTime = WAIT_MAX - (simTime() - waitingTime).dbl();
    bool isFull = false;
    if (step == 1) {
        waitingSize += (int)(((simTime() - waitingTime) * TX_SIZE * TX_SPEED).dbl());
        isFull = ((double)(BLOCK_MAX - waitingSize) / (TX_SIZE * TX_SPEED) < leftTime);
        if (waitingSize > BLOCK_MAX || isFull) {
            msgSize = BLOCK_MAX;
        } else if (leftTime < 0) {
            msgSize = max(min(waitingSize, BLOCK_MAX), 0);
        } else {
            msgSize = (int)(waitingSize + leftTime * TX_SIZE * TX_SPEED);
        }
    }
    if (step == 1 && isCommittee(getId() - 2, fromL, toL)) {
        //send_to_all(step, sign);
        send_to_id(step, sign, 0, CREATE_NUM);
        EV << "proposing block" << endl;
        lead_sign = sign;
        next_step();
    } else if (step == 2 && isCommittee(getId() - 2, fromC, toC) && !isCommittee(getId() - 2, fromL, toL) || step > 2 && isCommittee(getId() - 2, fromD, toD)) {
        //send_to_all(step, sign);
        if (step == 2) {
            send_to_id(step, sign, fromD, fromD + nD);
        } else {
            send_to_id(step, sign, 0, CREATE_NUM);
        }
    }
    if (step == 1) {
        EV << "leftTime: " << leftTime << endl;
        if (isFull) {
            waitingTime = simTime() + (double)max(BLOCK_MAX - waitingSize, 0) / (TX_SIZE * TX_SPEED);
            waitingSize = max(waitingSize - BLOCK_MAX, 0);
        } else if (leftTime < 0) {
            waitingTime = simTime();
            waitingSize = max(waitingSize - msgSize, 0);
        } else {
            waitingTime += WAIT_MAX;
            waitingSize = 0;
        }
        EV << "waitingSize: " << waitingSize << " delay: " << waitingTime - simTime() << endl;
    }
}

void Create::find_leader(uint32_t step) {
    int count = 0, fromL = newBlock.sign % CREATE_NUM, toL = (fromL + nL) % CREATE_NUM, leadId = RAND_MAX;
    for (json m : messages) {
        int sendId = m["sender"];
        if (m["round"] == round && m["step"] == 1 && isCommittee(sendId, fromL, toL)) {
            count++;
            if (fromL <= sendId && sendId < leadId || leadId < fromL && !(leadId < sendId && sendId < fromL)) {
                lead_sign = m["sign"];
                leadId = sendId;
            }
        }
    }
    if (leadId == fromL || simTime() - addingTime >= 2*SLAMBDA) {
        publish_message(2, lead_sign);
        next_step();
    }
}

void Create::step12() {
    newBlock.step = 1;
    isBot = false;
    publish_message(1, newBlock.sign);
    EV << "step2" << endl;
    newBlock.step++;
    lead_sign = RAND_MAX;
    json j = {{"round", round}, {"step", newBlock.step - 1}};
    scheduleAt(simTime() + 2*SLAMBDA, new cMessage(j.dump().c_str()));
    scheduleAt(simTime() + LLAMBDA, new cMessage(j.dump().c_str()));
}

void Create::mineBlock() {
    next_round = round + 1;
    char line_buffer[BUFFER_MAX] = "first";
    uint64_t size = strnlen(line_buffer, BUFFER_MAX) + 1;

    block *previous_ptr = NULL;
    newBlock = buildBlock(previous_ptr, line_buffer, size);
    srand(round);
    newBlock.sign = rand();
    step12();
}

bool Create::is_leader(json message, vector<json> value, float num) {
    int count = 0;
    int fromL = lead_sign % CREATE_NUM;
    int fromC = (fromL * round) % CREATE_NUM;
    int toC = (fromC + nD) % CREATE_NUM;
    for (json m : messages) {
        if (message["step"] == 2 && m["step"] == 1 && isCommittee(int(m["sender"]), fromC, toC)) { count++; }
    }
    for (json v : value) {
        if (message["sign"] == v["sign"] && message["step"] == v["step"]) { count++; }
        if (count >= num) { return true; }
    }
    //EV << "ID: " << getId() - 2 << ", count: " << count << endl;
    return false;
}

void Create::find_value(uint32_t step) {
    vector<json> value;
    for (json m : messages) {
        if (m["round"] == round && m["step"] == step - 1) {
            value.push_back(m);
            if (is_leader(m, value, tH)) {
                lead_sign = m["sign"];
                EV << "ID: " << getId() - 2 << ", sign: " << lead_sign << endl;
                publish_message(step, lead_sign);
                newBlock.step = max(step, newBlock.step);
                next_step();
                return;
            }
        }
    }
}

void Create::next_step() {
    if (newBlock.step > STEP_MAX) {
        finish_round();
        return;
    }
    newBlock.step++;
    EV << "ID: " << getId() - 2 << ", step" << newBlock.step << endl;
    find_value(newBlock.step);
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
    if (round >= next_round) { return; }
    if (!isBot && lead_sign < RAND_MAX) {
        EV << "verified!" << endl;
        createTime.collect(simTime() - addingTime);
        addingTime = simTime();
        blockSize += msgSize;
    }
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
    EV << "Total throughput Count:        " << throughput.getCount() << endl;
    EV << "Total Min throughput:          " << throughput.getMin() << endl;
    EV << "Total Mean throughput:         " << throughput.getMean() << endl;
    EV << "Total Max throughput:          " << throughput.getMax() << endl;
    EV << "Total throughput Std:          " << throughput.getStddev() << endl << endl;

    createTime.recordAs("create time");
    throughput.recordAs("throughput");
}

void Concentrate::handleMessage(cMessage *msg) {
    vector<int> receivers;
    simtime_t endTrsm;
    json m = json::parse(string(msg->getName()));
    vector<int> committee = m["committee"];
    m.erase("committee");
    int numPerConc = CREATE_NUM / CONC_NUM;
    int byteLength = dynamic_cast<cPacket *>(msg)->getByteLength();
    int gateId, id;
    for (int i = 0; i < committee.size(); i++) {
        id = committee[i];
        if (id / numPerConc == getId() - 2 - CREATE_NUM) {
            cPacket *pkt = new cPacket(m.dump().c_str());
            pkt->setByteLength(byteLength);
            /*
            if (m["step"] > 1 && int(m["sender"]) / numPerConc == getId() - 2 - CREATE_NUM) {
                pkt->setByteLength(byteLength * CREATE_NUM / CONC_NUM);
            } else {
                pkt->setByteLength(byteLength);
            }
            */
            gateId = id % numPerConc;
            endTrsm = gate("out", gateId)->getTransmissionChannel()->getTransmissionFinishTime();
            sendDelayed(pkt, max((endTrsm - simTime()).dbl(), 0.0), "out", gateId);
        } else {
            receivers.push_back(id);
            if ((id + 1) % numPerConc == 0 || i == committee.size() - 1 && receivers.size() > 0) {
                m["committee"] = receivers;
                cPacket *pkt = new cPacket(m.dump().c_str());
                pkt->setByteLength(byteLength);
                /*
                if (m["step"] > 1) {
                    pkt->setByteLength(byteLength * CREATE_NUM / CONC_NUM);
                } else {
                    pkt->setByteLength(byteLength);
                }
                */
                gateId = id / numPerConc;
                if (id / numPerConc > getId() - 2 - CREATE_NUM) { gateId--; }
                endTrsm = gate("gate$o", gateId)->getTransmissionChannel()->getTransmissionFinishTime();
                sendDelayed(pkt, max((endTrsm - simTime()).dbl(), 0.0), "gate$o", gateId);
                receivers.clear(); m.erase("committee");
            }
        }
    }
}
