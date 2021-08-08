/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Niket Agarwal
 *          Tushar Krishna
 */


#include "mem/ruby/network/garnet2.0/GarnetNetwork.hh"

#include <cassert>

#include "base/cast.hh"
#include "base/stl_helpers.hh"
#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/network/garnet2.0/CommonTypes.hh"
#include "mem/ruby/network/garnet2.0/CreditLink.hh"
#include "mem/ruby/network/garnet2.0/GarnetLink.hh"
#include "mem/ruby/network/garnet2.0/NetworkInterface.hh"
#include "mem/ruby/network/garnet2.0/NetworkLink.hh"
#include "mem/ruby/network/garnet2.0/Router.hh"
#include "mem/ruby/network/garnet2.0/InputUnit.hh"
#include "mem/ruby/network/garnet2.0/OutputUnit.hh"
#include "mem/ruby/network/garnet2.0/RoutingUnit.hh"
#include "mem/ruby/system/RubySystem.hh"

using namespace std;
using m5::stl_helpers::deletePointers;

/*
 * GarnetNetwork sets up the routers and links and collects stats.
 * Default parameters (GarnetNetwork.py) can be overwritten from command line
 * (see configs/network/Network.py)
 */

GarnetNetwork::GarnetNetwork(const Params *p)
    : Network(p)
{
    m_num_rows = p->num_rows;
    m_ni_flit_size = p->ni_flit_size;
    m_vcs_per_vnet = p->vcs_per_vnet;
    m_buffers_per_data_vc = p->buffers_per_data_vc;
    m_buffers_per_ctrl_vc = p->buffers_per_ctrl_vc;
    m_routing_algorithm = p->routing_algorithm;
    m_enable_bn = p->enable_bn;
    m_num_bubble = p->num_bubble;
    m_bubble_init = false;
    cout << "m_enable_bn: " << m_enable_bn << endl;
    cout << "m_num_bubble: " << m_num_bubble << endl;
    // not reserve but resize
    bubble.resize(m_num_bubble);
    m_intra_bubble_period = p->intra_period;
    m_inter_bubble_period = p->inter_period;
    cout << "m_intra_bubble_period: " << m_intra_bubble_period << endl;
    cout << "m_inter_bubble_period: " << m_inter_bubble_period << endl;
    last_inter_bubble_movement = Cycles(0);
    last_intra_bubble_movement = Cycles(0);
    m_total_packets_received = 0;
    warmup_cycles = p->warmup_cycles;
    marked_flits = p->marked_flits;
    marked_flt_injected = 0;
    marked_flt_received = 0;
    marked_pkt_received = 0;
    marked_pkt_injected = 0;
    total_marked_flit_latency = 0;
    total_marked_flit_received = 0;
    flit_latency = Cycles(0);
    flit_network_latency = Cycles(0);
    flit_queueing_latency = Cycles(0);
    marked_flit_latency = Cycles(0);
    marked_flit_network_latency = Cycles(0);
    marked_flit_queueing_latency = Cycles(0);
    rand_bb = p->rand_bb;
    m_scheme = p->scheme;
    cout << "m_scheme: " << m_scheme << endl;
//    assert(0);
    sim_type = p->sim_type;
    cout << "sim-type: " << sim_type << endl;
    // these should be later configured using command-line.
    // for now hard-codding them


    m_enable_fault_model = p->enable_fault_model;
    if (m_enable_fault_model)
        fault_model = p->fault_model;

    m_vnet_type.resize(m_virtual_networks);

    for (int i = 0 ; i < m_virtual_networks ; i++) {
        if (m_vnet_type_names[i] == "response")
            m_vnet_type[i] = DATA_VNET_; // carries data (and ctrl) packets
        else
            m_vnet_type[i] = CTRL_VNET_; // carries only ctrl packets
    }

    // record the routers
    for (vector<BasicRouter*>::const_iterator i =  p->routers.begin();
         i != p->routers.end(); ++i) {
        Router* router = safe_cast<Router*>(*i);
        m_routers.push_back(router);

        // initialize the router's network pointers
        router->init_net_ptr(this);
    }

    // record the network interfaces
    for (vector<ClockedObject*>::const_iterator i = p->netifs.begin();
         i != p->netifs.end(); ++i) {
        NetworkInterface *ni = safe_cast<NetworkInterface *>(*i);
        m_nis.push_back(ni);
        ni->init_net_ptr(this);
    }
    /*
    // initialize brownian bubbles here:
    for (int k=0; k < m_num_bubble; k++) {
        bubble.at(k).bubble_id = k;
        bubble.at(k).router_id = k % (m_routers.size());
        for (int inp_=0; inp_ < m_routers.at(bubble.at(k).router_id)->\
                                get_inputUnit_ref().size(); inp_++) {
            if (m_routers.at(bubble.at(k).router_id)->\
                get_inputUnit_ref().at(inp_)->get_direction() != "Local") {
                bubble.at(k).inport_dirn = m_routers.at(bubble.at(k).router_id)->\
                                        get_inputUnit_ref().at(inp_)->get_direction();
                bubble.at(k).inport_id = m_routers.at(bubble.at(k).router_id)->\
                                        get_inputUnit_ref().at(inp_)->get_id();
                assert(bubble.at(k).inport_id == m_routers[bubble[k].router_id]->\
                                                get_routingUnit_ref()->\
                                                get_inports_dirn2idx().at(bubble.at(k).inport_dirn));
                break;
            }

        }
    }
    // print out all the browninan_bubbles
    print_brownian_bubbles();
    assert(0);
    */
}

void
GarnetNetwork::init_brownian_bubbles() {
    // initialize them here
    // cout << "Control comes at init_brownian_bubbles()" << endl;
    // initialize brownian bubbles here:
    for (int k=0; k < m_num_bubble; k++) {
        bubble.at(k).bubble_id = k;
        if (rand_bb == 1) {
            try_again:
            bubble.at(k).router_id = random() % (m_routers.size());
            // so that no two bubbles get same router-ids
            for(int ii = 0; ii < k; ii++) {
                if (bubble.at(ii).router_id == bubble.at(k).router_id)
                    goto try_again;
            }
        } else {
            bubble.at(k).router_id = k % (m_routers.size());
        }
        cout << "bubble-router_id: " << bubble.at(k).router_id << endl;
        // initializie the sense of movement to be increaseing for all the
        // brownian-bubbles
        bubble.at(k).sense_inc = true;
        for (int inp_=0; inp_ < m_routers.at(bubble.at(k).router_id)->\
                                get_inputUnit_ref().size(); inp_++) {
            // Do the credit management
            if (m_routers.at(bubble.at(k).router_id)->\
                get_inputUnit_ref().at(inp_)->get_direction() != "Local") {
                bubble.at(k).inport_dirn = m_routers.at(bubble.at(k).router_id)->\
                                        get_inputUnit_ref().at(inp_)->get_direction();
                // bubble.at(k).last_inport_dirn = bubble.at(k).inport_dirn;
                bubble.at(k).last_inport_dirn = "Unknown";
                bubble.at(k).inport_id = m_routers.at(bubble.at(k).router_id)->\
                                        get_inputUnit_ref().at(inp_)->get_id();
                // bubble.at(k).last_inport_id = bubble.at(k).inport_id;
                bubble.at(k).last_inport_id = -1;
                //---------------- manage credits -----------------------//
                int inputPort = bubble[k].inport_id;
                int router_id = bubble[k].router_id;
                InputUnit* inpUnit = m_routers[router_id]->\
                                get_inputUnit_ref()[inputPort];
                OutputUnit* upstream_op_ = NULL;
                Router* upstream_router_ = m_routers[inpUnit->get_src_router()];
                for( int ii=0; ii <= upstream_router_->get_outputUnit_ref().size();
                                ii++ ) {
                    if(upstream_router_->get_outputUnit_ref()[ii]->get_dest_router() ==\
                        router_id) {
                        upstream_op_ = upstream_router_->get_outputUnit_ref()[ii];
                        break;
                    }
                }
                assert( upstream_op_ != NULL );
                upstream_op_->decrement_credit(0);
                // set vc-state to be active.
                upstream_op_->set_vc_state(ACTIVE_, 0, curCycle());
                // set this input vc to be active...
                inpUnit->set_vc_active(0, curCycle());
                //--------------- Credit management ends ----------------//
                assert(bubble.at(k).inport_id == m_routers[bubble[k].router_id]->\
                                                get_routingUnit_ref()->\
                                                get_inports_dirn2idx().at(bubble.at(k).inport_dirn));
                bubble.at(k).last_intra_movement_cycle = curCycle();
                bubble.at(k).last_inter_movement_cycle = curCycle();
                break;
            }

        }
    }
    // print out all the browninan_bubbles
    print_brownian_bubbles();
    print_topology();
    m_bubble_init = true;
    // assert(0);

}

void
GarnetNetwork::set_bubble_inport(int k, string dirn_) {
    // Do credit management here...
    for (int inp_=0; inp_ < m_routers[bubble[k].router_id]->\
                            get_inputUnit_ref().size(); inp_++) {
        // Do the credit management
        if (m_routers.at(bubble.at(k).router_id)->\
            get_inputUnit_ref().at(inp_)->get_direction() == dirn_) {
            bubble.at(k).inport_dirn = m_routers.at(bubble.at(k).router_id)->\
                                    get_inputUnit_ref().at(inp_)->get_direction();
            // bubble.at(k).last_inport_dirn = bubble.at(k).inport_dirn;
            bubble.at(k).last_inport_dirn = "Unknown";
            bubble.at(k).inport_id = m_routers.at(bubble.at(k).router_id)->\
                                    get_inputUnit_ref().at(inp_)->get_id();
            // bubble.at(k).last_inport_id = bubble.at(k).inport_id;
            bubble.at(k).last_inport_id = -1;
            //---------------- manage credits -----------------------//
            int inputPort = bubble[k].inport_id;
            int router_id = bubble[k].router_id;
            InputUnit* inpUnit = m_routers[router_id]->\
                            get_inputUnit_ref()[inputPort];
            OutputUnit* upstream_op_ = NULL;
            Router* upstream_router_ = m_routers[inpUnit->get_src_router()];
            for( int ii=0; ii <= upstream_router_->get_outputUnit_ref().size();
                            ii++ ) {
                if(upstream_router_->get_outputUnit_ref()[ii]->get_dest_router() ==\
                    router_id) {
                    upstream_op_ = upstream_router_->get_outputUnit_ref()[ii];
                    break;
                }
            }
            assert( upstream_op_ != NULL );
            upstream_op_->decrement_credit(0);
            // set vc-state to be active.
            upstream_op_->set_vc_state(ACTIVE_, 0, curCycle());
            // set this input vc to be active...
            inpUnit->set_vc_active(0, curCycle());
            //--------------- Credit management ends ----------------//
            assert(bubble.at(k).inport_id == m_routers[bubble[k].router_id]->\
                                            get_routingUnit_ref()->\
                                            get_inports_dirn2idx().at(bubble.at(k).inport_dirn));
            bubble.at(k).last_intra_movement_cycle = curCycle();
            bubble.at(k).last_inter_movement_cycle = curCycle();
            break;
        }
    }
}


void
GarnetNetwork::init_brownian_bubbles_cbs() {
    assert(m_num_bubble == 4*m_num_rows);
    for (int k=0; k < m_num_bubble; k++) {
        bubble.at(k).bubble_id = k;
        if (k >= 0 && k <= (m_num_rows - 1)) {
            bubble[k].router_id = k;
            // all south
            set_bubble_inport(k, "South");
        }
        else if ((k >= m_num_rows) && (k <= 2*m_num_rows - 1)) {
            bubble[k].router_id = k - m_num_rows;
            // all north
            set_bubble_inport(k, "North");
        }
        else if ((k >= 2*m_num_rows) && (k <= 3*m_num_rows - 1)) {
            bubble[k].router_id = (k % m_num_rows) * m_num_rows;
            // All West
            set_bubble_inport(k, "West");
        }
        else if ((k >= 3*m_num_rows) && (k <= 4*m_num_rows - 1)) {
            bubble[k].router_id = (k % m_num_rows) * m_num_rows;
            // All East
            set_bubble_inport(k, "East");
        }
        // All bubbles have been initialized to proper routers.

    }

    print_brownian_bubbles();
    m_bubble_init = true;
//    for (int ii = 0; ii < m_num_bubble; ii++) {
//        cout << bubble[ii].router_id << " ";
//        if ( (ii+1) % (m_num_rows) == 0 )
//            cout << endl;
//    }



}


bool
GarnetNetwork::move_intra_bubble(int bubble_id) {
    bool moved_ = false;
    int id = bubble_id;
    assert(curCycle() >= bubble[id].last_intra_movement_cycle);
    if ((curCycle() - bubble[id].last_intra_movement_cycle) >= m_intra_bubble_period) {
        // do the bubble movement here in a round-robin manner.
        int router_id = bubble[id].router_id;
        for (int itr = 0; itr < m_routers[router_id]->get_inputUnit_ref().size();
                        itr++) {
            // Randomly choose input unit.
            int inputPort =
                (random()%m_routers[router_id]->get_inputUnit_ref().size());
            InputUnit* inpUnit = m_routers[router_id]->get_inputUnit_ref()[inputPort];
            // cout << "inpUnit->get_direction: " << inpUnit->get_direction() << endl;
            if (inpUnit->get_direction() == "Local")
                continue;

            bool try_next = false;
            // make sure this empty input port vc is not another bubble
            // as there can be multiple bubble sitting within a router
            // scan through all brownian-bubbles:
            for (int bb=0; bb < bubble.size(); bb++) {
                if (id != bubble[bb].bubble_id) {
                    if ((bubble[bb].router_id == bubble[id].router_id) &&
                        (bubble[bb].inport_id == inpUnit->get_id())) {
                        try_next = true;
                        break;
                    }
                }
            }
            if (try_next)
                continue;
            // only move bubble to a input unit which has non-empty vc-0;
            // only condition when you cannot SWAP is when vc-0 is empty and
            // up-stream router's output unit has no credit.
            OutputUnit* upstream_op_ = NULL;
            Router* upstream_router_ = m_routers[inpUnit->get_src_router()];
            for( int ii=0; ii <= upstream_router_->get_outputUnit_ref().size();
                            ii++ ) {
                if(upstream_router_->get_outputUnit_ref()[ii]->get_dest_router() ==\
                    router_id) {
                    upstream_op_ = upstream_router_->get_outputUnit_ref()[ii];
                    break;
                }
            }
            assert( upstream_op_ != NULL );
            InputUnit* orig_inpUnit = m_routers[router_id]->\
                            get_inputUnit_ref()[bubble[id].inport_id];
            // original output unit
            OutputUnit* orig_upstream_op_ = NULL;
            Router* orig_upstream_router_ = m_routers[orig_inpUnit->get_src_router()];
            for( int ii=0; ii <= orig_upstream_router_->get_outputUnit_ref().size();
                            ii++ ) {
                if(orig_upstream_router_->get_outputUnit_ref()[ii]->get_dest_router() ==\
                    router_id) {
                    orig_upstream_op_ = orig_upstream_router_->get_outputUnit_ref()[ii];
                    break;
                }
            }
            assert(orig_upstream_op_ != NULL);
            if ( (inpUnit->vc_isEmpty(0) == true) &&
                (upstream_op_->is_vc_idle(0, curCycle()) == false)) {
                // this means that the flit is on the link.. or going to be
                // on the link in next cycle OR,
                // Input Unit just got free.. and its credit is on the link.
                // find the next input port
                continue;
            }
            else {
                // exchange with an empty input port.
                if( (inpUnit->vc_isEmpty(0) == true) &&
                    ( upstream_op_->is_vc_idle(0, curCycle()) == true) ) {

                    // decrement credit and update the bubble and break.
                    upstream_op_->decrement_credit(0);
                    // set vc-state to be active.
                    upstream_op_->set_vc_state(ACTIVE_, 0, curCycle());
                    // set this input vc to be active...
                    inpUnit->set_vc_active(0, curCycle());
                    // increment the credits where bubble was originally present

                    orig_upstream_op_->increment_credit(0);
                    orig_upstream_op_->set_vc_state(IDLE_, 0, curCycle());
                    orig_inpUnit->set_vc_idle(0, curCycle());

                    // update the bubble and break
                    bubble[id].last_inport_id = bubble[id].inport_id;
                    bubble[id].last_inport_dirn = bubble[id].inport_dirn;
                    bubble[id].inport_id = inpUnit->get_id();
                    bubble[id].inport_dirn = inpUnit->get_direction();
                    assert(bubble[id].inport_dirn != "Local");
                    bubble[id].last_intra_movement_cycle = curCycle();
                    moved_ = true;
                    // update the stats here...
                    m_num_intra_swap++;
                    m_intra_swap_bubble++;
                    // schedule wakeup for this router at next inter-bubble-period.

                    if (curCycle() > Cycles(m_inter_bubble_period)) {
                        m_routers[router_id]->\
                        schedule_wakeup( Cycles(m_inter_bubble_period) - \
                        Cycles( uint64_t(curCycle()) % m_inter_bubble_period));
                    }
                    break;
                }
                //exchange with a packet.. no credit management is needed.
                else if ( (inpUnit->vc_isEmpty(0) == false) &&
                        (upstream_op_->is_vc_idle(0, curCycle()) == false) ) {
                    flit* t_flit = inpUnit->getTopFlit(0);
                    // insert the flit at the location pointed out by the bubble
                    orig_inpUnit->insertFlit(0, t_flit);
                    // no credit management needed.
                    // update the bubble and break
                    bubble[id].last_inport_id = bubble[id].inport_id;
                    bubble[id].last_inport_dirn = bubble[id].inport_dirn;
                    bubble[id].inport_id = inpUnit->get_id();
                    bubble[id].inport_dirn = inpUnit->get_direction();
                    assert(bubble[id].inport_dirn != "Local");
                    bubble[id].last_intra_movement_cycle = curCycle();
                    moved_ = true;
                    // update stats here:
                    m_num_intra_swap++;
                    m_intra_swap_pkt++;
                    break;
                }
                else {
                    assert(0);
                }
            }
        }
    }
    return moved_;
}

bool
GarnetNetwork::move_inter_bubble(int bubble_id) {
    bool moved_ = false;
    // int id = bubble_id;
    // assert()
    // Todo: Fill this in.
    // you may exhange it with a critical bubble if it comes to that..
    // other wise there might be deadlock.. [ have not thought it through ]
    assert(curCycle() >= bubble[bubble_id].last_inter_movement_cycle);

    if ((curCycle() - bubble[bubble_id].last_inter_movement_cycle) >=\
            m_inter_bubble_period) {
        int nxt_router_id = move_next_router(bubble[bubble_id].router_id,
                                            bubble_id);
        Router* nxt_router = m_routers[nxt_router_id];
        // get the input unit of this router.
        for (int itr=0; itr < nxt_router->get_inputUnit_ref().size();
            itr++) {
            int inputPort = (random() %
                nxt_router->get_inputUnit_ref().size());
            // this 'inpUnit' is the input unit of the next router
            InputUnit* inpUnit = nxt_router->get_inputUnit_ref()[inputPort];
            if ( inpUnit->get_direction() == "Local" )
                continue;
            // If control comes here then check for the three possible condition.
            // Case I: if the vc-0 is empty and not a brownian bubble
            // check if that input unit's upstream router has this vc as idle.
            OutputUnit* upstream_op_ = NULL; // this 'upstream_op_' is output unit of
                                            // upstream router of the next router
            Router* upstream_nxt_router = m_routers[inpUnit->get_src_router()];
            for (int ii=0; ii <= upstream_nxt_router->get_outputUnit_ref().size();
                    ii++) {
                if(upstream_nxt_router->get_outputUnit_ref()[ii]->get_dest_router()
                    == nxt_router_id) {
                    upstream_op_ = upstream_nxt_router->get_outputUnit_ref()[ii];
                    break;
                }
            }
            assert( upstream_op_ != NULL );
            // Original input-unit/ output unit; where bubble is present.
            ////////////////////////////////////////////////
            InputUnit* orig_inpUnit = m_routers[bubble[bubble_id].router_id]->\
                            get_inputUnit_ref()[bubble[bubble_id].inport_id];
            // original output unit
            OutputUnit* orig_upstream_op_ = NULL;
            Router* orig_upstream_router_ = m_routers[orig_inpUnit->get_src_router()];
            for( int ii=0; ii <= orig_upstream_router_->get_outputUnit_ref().size();
                            ii++ ) {
                if(orig_upstream_router_->get_outputUnit_ref()[ii]->get_dest_router() ==\
                    bubble[bubble_id].router_id) {
                    orig_upstream_op_ = orig_upstream_router_->get_outputUnit_ref()[ii];
                    break;
                }
            }
            assert(orig_upstream_op_ != NULL);
            ////////////////////////////////////////////////
            if ((inpUnit->vc_isEmpty(0) == true) &&
                (upstream_op_->is_vc_idle(0, curCycle()) == true)) {
                // decrement credit and update the bubble and break
                upstream_op_->decrement_credit(0);
                // set vc-state to be active.
                upstream_op_->set_vc_state(ACTIVE_, 0, curCycle());
                // set this input vc (of the next router) to be active...
                inpUnit->set_vc_active(0, curCycle());
                // increment the credits where bubble was originally present
                orig_upstream_op_->increment_credit(0);
                orig_upstream_op_->set_vc_state(IDLE_, 0, curCycle());
                orig_inpUnit->set_vc_idle(0, curCycle());
                Router* orig_router = m_routers[bubble[bubble_id].router_id];
                orig_router->schedule_wakeup(Cycles(2)); // 2 cycles from now.
                nxt_router->schedule_wakeup(Cycles(2)); // 2 cycles from now.


                // update the bubble and break...
                bubble[bubble_id].last_inport_id = bubble[bubble_id].inport_id;
                bubble[bubble_id].last_inport_dirn = bubble[bubble_id].inport_dirn;
                bubble[bubble_id].router_id = nxt_router_id;
                bubble[bubble_id].inport_id = inpUnit->get_id();
                bubble[bubble_id].inport_dirn = inpUnit->get_direction();
                assert(bubble[bubble_id].inport_dirn != "Local");
                bubble[bubble_id].last_inter_movement_cycle = curCycle();
                moved_ = true;
                // Update stats here:
                m_num_inter_swap++;
                m_inter_swap_bubble++;
                break;
            }

            // Case II: if the vc-0 is empty and a brownian bubble as well.
            // then don't do anything here...
            else if ((inpUnit->vc_isEmpty(0) == true) &&
                    (upstream_op_->is_vc_idle(0, curCycle()) == false)) {
                // try any other input port
                continue; // easy fix for now
            }
            // Case III: if vc-0 has a packet sitting.
            else if ((inpUnit->vc_isEmpty(0) == false) &&
                    (upstream_op_->is_vc_idle(0, curCycle()) == false)) {
                flit* t_flit = inpUnit->peekTopFlit(0);
                if (t_flit->get_outport_dirn() == "Local")
                    continue;
                // set flit time here:
                t_flit = inpUnit->getTopFlit(0);
                // re-compute the route from orig router and add it to this flit
                Router* orig_router = m_routers[bubble[bubble_id].router_id];
                int outport = orig_router->route_compute(t_flit->get_route(),
                            orig_inpUnit->get_id(), orig_inpUnit->get_direction());
                t_flit->set_outport(outport);
                PortDirection out_dirn = orig_router->getOutportDirection(outport);
                t_flit->set_outport_dirn(out_dirn);
                t_flit->set_time(curCycle() + Cycles(2));
                // insert the flit at the location pointed out by the bubble
                orig_inpUnit->insertFlit(0, t_flit);
                orig_router->schedule_wakeup(Cycles(2)); // 2 cycles from now.
                nxt_router->schedule_wakeup(Cycles(2)); // 2 cycles from now.

                // no credit management needed.
                // update the bubble and break.
                bubble[bubble_id].last_inport_id = bubble[bubble_id].inport_id;
                bubble[bubble_id].last_inport_dirn = bubble[bubble_id].inport_dirn;
                bubble[bubble_id].router_id = nxt_router_id;
                bubble[bubble_id].inport_id = inpUnit->get_id();
                bubble[bubble_id].inport_dirn = inpUnit->get_direction();
                assert(bubble[bubble_id].inport_dirn != "Local");
                bubble[bubble_id].last_inter_movement_cycle = curCycle();
                moved_ = true;
                m_num_inter_swap++;
                m_inter_swap_pkt++;
                break;
            }
        }
    }
    // select and input port from next router
    return moved_;
}

bool
GarnetNetwork::move_inter_bubble_cbs(int bubble_id) {
    bool moved_ = false;
    // int id = bubble_id;
    // assert()
    // Todo: Fill this in.
    // you may exhange it with a critical bubble if it comes to that..
    // other wise there might be deadlock.. [ have not thought it through ]
    assert(curCycle() >= bubble[bubble_id].last_inter_movement_cycle);
    int mesh_row = m_num_rows;
    if ((curCycle() - bubble[bubble_id].last_inter_movement_cycle) >=\
            m_inter_bubble_period) {
        int nxt_router_id = move_next_router_cbs(bubble[bubble_id].router_id,
                                            bubble_id);
        Router* nxt_router = m_routers[nxt_router_id];
        // get the input unit of this router.
        InputUnit* inpUnit = NULL;
        for (int itr=0; itr < nxt_router->get_inputUnit_ref().size();
            itr++) {
            for (int inport_=0; inport_ < nxt_router->get_inputUnit_ref().size(); inport_++ ) {
                inpUnit = nxt_router->get_inputUnit_ref()[inport_];
                if (bubble_id >= 0 && bubble_id <= mesh_row - 1) {
                    if(inpUnit->get_direction() == "South")
                        break;
                }
                else if ( bubble_id >= mesh_row && bubble_id <= 2*mesh_row - 1 ) {
                    if (inpUnit->get_direction() == "North")
                        break;
                }
                else if ( bubble_id >= 2*mesh_row && bubble_id <= 3*mesh_row - 1 ) {
                    if (inpUnit->get_direction() == "East")
                        break;
                }
                else if ( bubble_id >= 3*mesh_row && bubble_id <= 4*mesh_row -1 ) {
                    if (inpUnit->get_direction() == "West")
                        break;
                }
            }
            assert(inpUnit != NULL);
            OutputUnit* upstream_op_ = NULL; // this 'upstream_op_' is output unit of
                                            // upstream router of the next router
            Router* upstream_nxt_router = m_routers[inpUnit->get_src_router()];
            for (int ii=0; ii <= upstream_nxt_router->get_outputUnit_ref().size();
                    ii++) {
                if(upstream_nxt_router->get_outputUnit_ref()[ii]->get_dest_router()
                    == nxt_router_id) {
                    upstream_op_ = upstream_nxt_router->get_outputUnit_ref()[ii];
                    break;
                }
            }
            assert( upstream_op_ != NULL );
            // Original input-unit/ output unit; where bubble is present.
            ////////////////////////////////////////////////
            InputUnit* orig_inpUnit = m_routers[bubble[bubble_id].router_id]->\
                            get_inputUnit_ref()[bubble[bubble_id].inport_id];
            // original output unit
            OutputUnit* orig_upstream_op_ = NULL;
            Router* orig_upstream_router_ = m_routers[orig_inpUnit->get_src_router()];
            for( int ii=0; ii <= orig_upstream_router_->get_outputUnit_ref().size();
                            ii++ ) {
                if(orig_upstream_router_->get_outputUnit_ref()[ii]->get_dest_router() ==\
                    bubble[bubble_id].router_id) {
                    orig_upstream_op_ = orig_upstream_router_->get_outputUnit_ref()[ii];
                    break;
                }
            }
            assert(orig_upstream_op_ != NULL);
            ////////////////////////////////////////////////
            if ((inpUnit->vc_isEmpty(0) == true) &&
                (upstream_op_->is_vc_idle(0, curCycle()) == true)) {
                // decrement credit and update the bubble and break
                upstream_op_->decrement_credit(0);
                // set vc-state to be active.
                upstream_op_->set_vc_state(ACTIVE_, 0, curCycle());
                // set this input vc (of the next router) to be active...
                inpUnit->set_vc_active(0, curCycle());
                // increment the credits where bubble was originally present
                orig_upstream_op_->increment_credit(0);
                orig_upstream_op_->set_vc_state(IDLE_, 0, curCycle());
                orig_inpUnit->set_vc_idle(0, curCycle());
                Router* orig_router = m_routers[bubble[bubble_id].router_id];
                orig_router->schedule_wakeup(Cycles(2)); // 2 cycles from now.
                nxt_router->schedule_wakeup(Cycles(2)); // 2 cycles from now.

                // update the bubble and break...
                bubble[bubble_id].last_inport_id = bubble[bubble_id].inport_id;
                bubble[bubble_id].last_inport_dirn = bubble[bubble_id].inport_dirn;
                bubble[bubble_id].router_id = nxt_router_id;
                bubble[bubble_id].inport_id = inpUnit->get_id();
                bubble[bubble_id].inport_dirn = inpUnit->get_direction();
                assert(bubble[bubble_id].inport_dirn != "Local");
                bubble[bubble_id].last_inter_movement_cycle = curCycle();
                moved_ = true;
                // Update stats here:
                m_num_inter_swap++;
                m_inter_swap_bubble++;
                break;
            }

            // Case II: if the vc-0 is empty and a brownian bubble as well.
            // then don't do anything here...
            else if ((inpUnit->vc_isEmpty(0) == true) &&
                    (upstream_op_->is_vc_idle(0, curCycle()) == false)) {
                // try any other input port
                continue; // easy fix for now
            }
            // Case III: if vc-0 has a packet sitting.
            else if ((inpUnit->vc_isEmpty(0) == false) &&
                    (upstream_op_->is_vc_idle(0, curCycle()) == false)) {
                flit* t_flit = inpUnit->peekTopFlit(0);
                if (t_flit->get_outport_dirn() == "Local")
                    continue;
                t_flit = inpUnit->getTopFlit(0);
                // re-compute the route from orig router and add it to this flit
                Router* orig_router = m_routers[bubble[bubble_id].router_id];
                int outport = orig_router->route_compute(t_flit->get_route(),
                            orig_inpUnit->get_id(), orig_inpUnit->get_direction());
                t_flit->set_outport(outport);
                PortDirection out_dirn = orig_router->getOutportDirection(outport);
                t_flit->set_outport_dirn(out_dirn);
                t_flit->set_time(curCycle() + Cycles(2));
                // insert the flit at the location pointed out by the bubble
                orig_inpUnit->insertFlit(0, t_flit);
                orig_router->schedule_wakeup(Cycles(2)); // 2 cycles from now.
                nxt_router->schedule_wakeup(Cycles(2)); // 2 cycles from now.
                // no credit management needed.
                // update the bubble and break.
                bubble[bubble_id].last_inport_id = bubble[bubble_id].inport_id;
                bubble[bubble_id].last_inport_dirn = bubble[bubble_id].inport_dirn;
                bubble[bubble_id].router_id = nxt_router_id;
                bubble[bubble_id].inport_id = inpUnit->get_id();
                bubble[bubble_id].inport_dirn = inpUnit->get_direction();
                assert(bubble[bubble_id].inport_dirn != "Local");
                bubble[bubble_id].last_inter_movement_cycle = curCycle();
                moved_ = true;
                m_num_inter_swap++;
                m_inter_swap_pkt++;
                break;
            }
        }
    }
    return moved_;
}

int
GarnetNetwork::move_next_router_cbs(int curr_router_id, int bubble_id) {
    int next_router_id = -1;
    int mesh_row = m_num_rows;
    if((bubble_id >=  0) && (bubble_id <= 2*m_num_rows - 1)) {
        if ( curr_router_id / mesh_row == mesh_row -1 ) {
            next_router_id = curr_router_id % mesh_row;
        }
        else {
            next_router_id = curr_router_id + mesh_row;
        }
    }
    else if ((bubble_id >= 2*m_num_rows) && (bubble_id <= 4*m_num_rows - 1)) {
        if ( curr_router_id % mesh_row == mesh_row - 1) {
            next_router_id = curr_router_id - mesh_row + 1;
        }
        else {
            next_router_id = curr_router_id + 1;
        }
    }
    assert (next_router_id >= 0);
    assert (next_router_id < m_routers.size());
    return (next_router_id);
}

int
GarnetNetwork::move_next_router(int curr_router_id, int bubble_id) {
    int next_router_id = -1;
        // curr_router_id; // this will be router-id of the bubble
    int mesh_row = m_num_rows; // this will be topology row
    // bool sense_inc = true; // this will be member variable of bubble

    if (curr_router_id == mesh_row*mesh_row - mesh_row)
        bubble[bubble_id].sense_inc = false;
    if (curr_router_id == 0)
        bubble[bubble_id].sense_inc = true;

    if (bubble[bubble_id].sense_inc) {

        if ((curr_router_id/mesh_row) % 2 == 0) {
            // this means this is an even mesh_row
            // starting from 0.
            if (curr_router_id%mesh_row == mesh_row-1) {
                next_router_id = curr_router_id + mesh_row;
            }
            else
                next_router_id = curr_router_id + 1;
        }
        else if ((curr_router_id/mesh_row) % 2 == 1) {
            // this means this is an odd mesh_row
            // starting from 0.
            if (curr_router_id%mesh_row == 0) {
                // cout << "next next_router_id: " << next_router_id << endl;
                next_router_id = curr_router_id + mesh_row;
            }
            else
                next_router_id = curr_router_id - 1;
        }
    }
    else if (!bubble[bubble_id].sense_inc) {
        if ((curr_router_id/mesh_row) % 2 == 0) {
            // this means this is an even mesh_row
            // starting from 0.
            if (curr_router_id%mesh_row == 0) {

                next_router_id = curr_router_id - mesh_row;
            }
            else
                next_router_id = curr_router_id - 1;
        }
        else if ((curr_router_id/mesh_row) % 2 == 1) {
            // this means this is an odd mesh_row
            // starting from 0.
            if (curr_router_id%mesh_row == mesh_row-1) {
                // cout << "next next_router_id: " << next_router_id << endl;
                next_router_id = curr_router_id - mesh_row;
            }
            else
                next_router_id = curr_router_id + 1;
        }
    }
    // cout << "curr_router_id: " << curr_router_id << endl;
    // cout << "next_router_id: " << next_router_id  << endl;
    assert (next_router_id >= 0);
    assert (next_router_id < m_routers.size());
    return (next_router_id);
}

void
GarnetNetwork::print_topology() {

    for (int k = 0; k < m_routers.size(); k++) {
        cout << "  *************  " << endl;
        cout << "router_id: " << m_routers[k]->get_id() << endl;
        cout << "  -------------  " << endl;

        for(int inp=0; inp < m_routers[k]->\
            get_inputUnit_ref().size(); inp++) {
            InputUnit *inp_ = m_routers[k]->get_inputUnit_ref()[inp];
            cout << "Input-Unit id: " << inp_->get_id() << endl;
            cout << "Input-Unit direction: " << inp_->get_direction() << endl;
            cout << "Input-Unit source-router id: " << inp_->get_src_router() << endl;
        }

        for(int out=0; out < m_routers[k]->\
            get_outputUnit_ref().size(); out++) {
            OutputUnit *out_ = m_routers[k]->get_outputUnit_ref()[out];
            cout << "Output-Unit id: " << out_->get_id() << endl;
            cout << "Output-Unit direction: " << out_->get_direction() << endl;
            cout << "Output-Unit destination-router id: " << out_->get_dest_router() << endl;
        }
    }
}


bool
GarnetNetwork::check_mrkd_flt()
{
    int itr = 0;
    for(itr = 0; itr < m_routers.size(); ++itr) {
      if(m_routers.at(itr)->mrkd_flt_ > 0)
          break;
    }

    if(itr < m_routers.size()) {
      return false;
    }
    else {
        if(marked_flt_received < marked_flt_injected )
            return false;
        else
            return true;
    }
}


void
GarnetNetwork::print_brownian_bubbles() {

    for (int idx = 0; idx < bubble.size(); idx++) {
        cout << "[ bubble-" << bubble.at(idx).bubble_id << " ";
        cout << "router-id: " << bubble.at(idx).router_id << " ";
        cout << "inport-id: " << bubble.at(idx).inport_id << " ";
        cout << "inport-dirn: " << bubble.at(idx).inport_dirn << " ]";
        cout << endl;
    }
}

Router*
GarnetNetwork::get_RouterInDirn( PortDirection outport_dir, int my_id )
{
    int num_cols = getNumCols();
    int downstream_id = -1; // router_id for downstream router
//    cout << "GarnetNetwork::get_RouterInDirn: outport_dir: " << outport_dir << endl;
//    cout << "GarnetNetwork::get_RouterInDirn: my_id: " << my_id << endl;
    // Do border control check here...

    /*outport direction fromt he flit for this router*/
    if (outport_dir == "East") {
        downstream_id = my_id + 1;
        // constraints on downstream router-id
        for(int k=(num_cols-1); k < getNumRouters(); k+=num_cols ) {
            assert(my_id != k);
        }
    }
    else if (outport_dir == "West") {
        downstream_id = my_id - 1;
        // constraints on downstream router-id
        for(int k=0; k < getNumRouters(); k+=num_cols ) {
            assert(my_id != k);
        }
    }
    else if (outport_dir == "North") {
        downstream_id = my_id + num_cols;
        // constraints on downstream router-id
        for(int k=((num_cols-1)*num_cols); k < getNumRouters(); k+=1 ) {
            assert(my_id != k);
        }
    }
    else if (outport_dir == "South") {
        downstream_id = my_id - num_cols;
        // constraints on downstream router-id
        for(int k=0; k < num_cols; k+=1 ) {
            assert(my_id != k);
        }
    }
    else if (outport_dir == "Local"){
        assert(0);
        return NULL;
    }
    else {
        assert(0); // for completion of if-else chain
        return NULL;
    }

//    cout << "GarnetNetwork::get_RouterInDirn: downstream_id: " << downstream_id << endl;
//    cout << "GarnetNetwork::get_RouterInDirn: my_id: " << my_id << endl;
//    cout << "----------------" << endl;

    if ((downstream_id < 0) || (downstream_id >= getNumRouters())) {
        assert(0);
        return NULL;
    } else
        return m_routers[downstream_id];
//    return downstream_id;
}


void
GarnetNetwork::init()
{
    Network::init();

    for (int i=0; i < m_nodes; i++) {
        m_nis[i]->addNode(m_toNetQueues[i], m_fromNetQueues[i]);
    }

    // The topology pointer should have already been initialized in the
    // parent network constructor
    assert(m_topology_ptr != NULL);
    m_topology_ptr->createLinks(this);

    // Initialize topology specific parameters
    if (getNumRows() > 0) {
        // Only for Mesh topology
        // m_num_rows and m_num_cols are only used for
        // implementing XY or custom routing in RoutingUnit.cc
        m_num_rows = getNumRows();
        m_num_cols = m_routers.size() / m_num_rows;
        assert(m_num_rows * m_num_cols == m_routers.size());
    } else {
        m_num_rows = -1;
        m_num_cols = -1;
    }

    // FaultModel: declare each router to the fault model
    if (isFaultModelEnabled()) {
        for (vector<Router*>::const_iterator i= m_routers.begin();
             i != m_routers.end(); ++i) {
            Router* router = safe_cast<Router*>(*i);
            int router_id M5_VAR_USED =
                fault_model->declare_router(router->get_num_inports(),
                                            router->get_num_outports(),
                                            router->get_vc_per_vnet(),
                                            getBuffersPerDataVC(),
                                            getBuffersPerCtrlVC());
            assert(router_id == router->get_id());
            router->printAggregateFaultProbability(cout);
            router->printFaultVector(cout);
        }
    }
    cout << "print for GarnetNetwork::init()" << endl;
    /*
    // initialize brownian bubbles here:
    for (int k=0; k < m_num_bubble; k++) {
        bubble.at(k).bubble_id = k;
        bubble.at(k).router_id = k % (m_routers.size());

        for (int inp_=0; inp_ < m_routers.at(bubble.at(k).router_id)->\
                                get_inputUnit_ref().size(); inp_++) {
            if (m_routers.at(bubble.at(k).router_id)->\
                get_inputUnit_ref().at(inp_)->get_direction() != "Local") {
                bubble.at(k).inport_dirn = m_routers.at(bubble.at(k).router_id)->\
                                        get_inputUnit_ref().at(inp_)->get_direction();
                bubble.at(k).inport_id = m_routers.at(bubble.at(k).router_id)->\
                                        get_inputUnit_ref().at(inp_)->get_id();
                assert(bubble.at(k).inport_id == m_routers[bubble[k].router_id]->\
                                                get_routingUnit_ref()->\
                                                get_inports_dirn2idx().at(bubble.at(k).inport_dirn));
                break;
            }

        }
    }
    // print out all the browninan_bubbles
    print_brownian_bubbles();
    assert(0);
    */
}

GarnetNetwork::~GarnetNetwork()
{
    deletePointers(m_routers);
    deletePointers(m_nis);
    deletePointers(m_networklinks);
    deletePointers(m_creditlinks);
}

/*
 * This function creates a link from the Network Interface (NI)
 * into the Network.
 * It creates a Network Link from the NI to a Router and a Credit Link from
 * the Router to the NI
*/

void
GarnetNetwork::makeExtInLink(NodeID src, SwitchID dest, BasicLink* link,
                            const NetDest& routing_table_entry)
{
    assert(src < m_nodes);

    GarnetExtLink* garnet_link = safe_cast<GarnetExtLink*>(link);

    // GarnetExtLink is bi-directional
    NetworkLink* net_link = garnet_link->m_network_links[LinkDirection_In];
    net_link->setType(EXT_IN_);
    CreditLink* credit_link = garnet_link->m_credit_links[LinkDirection_In];

    m_networklinks.push_back(net_link);
    m_creditlinks.push_back(credit_link);

    PortDirection dst_inport_dirn = "Local";
    m_routers[dest]->addInPort(dst_inport_dirn, net_link, credit_link);
    m_nis[src]->addOutPort(net_link, credit_link, dest);
}

/*
 * This function creates a link from the Network to a NI.
 * It creates a Network Link from a Router to the NI and
 * a Credit Link from NI to the Router
*/

void
GarnetNetwork::makeExtOutLink(SwitchID src, NodeID dest, BasicLink* link,
                             const NetDest& routing_table_entry)
{
    assert(dest < m_nodes);
    assert(src < m_routers.size());
    assert(m_routers[src] != NULL);

    GarnetExtLink* garnet_link = safe_cast<GarnetExtLink*>(link);

    // GarnetExtLink is bi-directional
    NetworkLink* net_link = garnet_link->m_network_links[LinkDirection_Out];
    net_link->setType(EXT_OUT_);
    CreditLink* credit_link = garnet_link->m_credit_links[LinkDirection_Out];

    m_networklinks.push_back(net_link);
    m_creditlinks.push_back(credit_link);

    PortDirection src_outport_dirn = "Local";
    m_routers[src]->addOutPort(src_outport_dirn, net_link,
                               routing_table_entry,
                               link->m_weight, credit_link);
    m_nis[dest]->addInPort(net_link, credit_link);
}

/*
 * This function creates an internal network link between two routers.
 * It adds both the network link and an opposite credit link.
*/

void
GarnetNetwork::makeInternalLink(SwitchID src, SwitchID dest, BasicLink* link,
                                const NetDest& routing_table_entry,
                                PortDirection src_outport_dirn,
                                PortDirection dst_inport_dirn)
{
    GarnetIntLink* garnet_link = safe_cast<GarnetIntLink*>(link);

    // GarnetIntLink is unidirectional
    NetworkLink* net_link = garnet_link->m_network_link;
    net_link->setType(INT_);
    CreditLink* credit_link = garnet_link->m_credit_link;
    net_link->set_link_src_dest(src, dest);
    m_networklinks.push_back(net_link);
    m_creditlinks.push_back(credit_link);

    m_routers[dest]->addInPort(dst_inport_dirn, net_link, credit_link);
    m_routers[src]->addOutPort(src_outport_dirn, net_link,
                               routing_table_entry,
                               link->m_weight, credit_link);
}

// Total routers in the network
int
GarnetNetwork::getNumRouters()
{
    return m_routers.size();
}

// Get ID of router connected to a NI.
int
GarnetNetwork::get_router_id(int ni)
{
    return m_nis[ni]->get_router_id();
}

void
GarnetNetwork::regStats()
{
    Network::regStats();

    m_pre_mature_exit
        .name(name() + ".pre_mature_exit");

    m_marked_flt_dist
        .init(m_routers.size())
        .name(name() + ".marked_flit_distribution")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flt_dist
        .init(m_routers.size())
        .name(name() + ".flit_distribution")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;


    m_network_latency_histogram
        .init(21)
        .name(name() + ".network_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flt_latency_hist
        .init(100)
        .name(name() + ".flit_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flt_network_latency_hist
        .init(100)
        .name(name() + ".flit_network_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flt_queueing_latency_hist
        .init(100)
        .name(name() + ".flit_queueing_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_marked_flt_latency_hist
        .init(100)
        .name(name() + ".marked_flit_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;


    m_marked_flt_network_latency_hist
        .init(100)
        .name(name() + ".marked_flit_network_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_marked_flt_queueing_latency_hist
        .init(100)
        .name(name() + ".marked_flit_queueing_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;


    m_num_inter_swap
        .name(name() + ".total_inter_swap");
    m_num_intra_swap
        .name(name() + ".total_intra_swap");
    m_inter_swap_bubble
        .name(name() + ".inter_bubble_swap");
    m_intra_swap_bubble
        .name(name() + ".intra_bubble_swap");
    m_inter_swap_pkt
        .name(name() + ".inter_swap_pkt");
    m_intra_swap_pkt
        .name(name() + ".intra_swap_pkt");

    // Packets
    m_packets_received
        .init(m_virtual_networks)
        .name(name() + ".packets_received")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_packets_injected
        .init(m_virtual_networks)
        .name(name() + ".packets_injected")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_packet_network_latency
        .init(m_virtual_networks)
        .name(name() + ".packet_network_latency")
        .flags(Stats::oneline)
        ;

    m_packet_queueing_latency
        .init(m_virtual_networks)
        .name(name() + ".packet_queueing_latency")
        .flags(Stats::oneline)
        ;

    m_max_flit_latency
        .name(name() + ".max_flit_latency");
    m_max_flit_network_latency
        .name(name() + ".max_flit_network_latency");
    m_max_flit_queueing_latency
        .name(name() + ".max_flit_queueing_latency");
    m_max_marked_flit_latency
        .name(name() + ".max_marked_flit_latency");
    m_max_marked_flit_network_latency
        .name(name() + ".max_marked_flit_network_latency");
    m_max_marked_flit_queueing_latency
        .name(name() + ".max_marked_flit_queueing_latency");

    m_marked_pkt_received
        .init(m_virtual_networks)
        .name(name() + ".marked_pkt_receivced")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
    m_marked_pkt_injected
        .init(m_virtual_networks)
        .name(name() + ".marked_pkt_injected")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
    m_marked_pkt_network_latency
        .init(m_virtual_networks)
        .name(name() + ".marked_pkt_network_latency")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
    m_marked_pkt_queueing_latency
        .init(m_virtual_networks)
        .name(name() + ".marked_pkt_queueing_latency")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    for (int i = 0; i < m_virtual_networks; i++) {
        m_packets_received.subname(i, csprintf("vnet-%i", i));
        m_packets_injected.subname(i, csprintf("vnet-%i", i));
        m_marked_pkt_injected.subname(i, csprintf("vnet-%i", i));
        m_marked_pkt_received.subname(i, csprintf("vnet-%i", i));
        m_packet_network_latency.subname(i, csprintf("vnet-%i", i));
        m_packet_queueing_latency.subname(i, csprintf("vnet-%i", i));
        m_marked_pkt_network_latency.subname(i, csprintf("vnet-%i", i));
        m_marked_pkt_queueing_latency.subname(i, csprintf("vnet-%i", i));
    }

    m_avg_packet_vnet_latency
        .name(name() + ".average_packet_vnet_latency")
        .flags(Stats::oneline);
    m_avg_packet_vnet_latency =
        m_packet_network_latency / m_packets_received;

    m_avg_packet_vqueue_latency
        .name(name() + ".average_packet_vqueue_latency")
        .flags(Stats::oneline);
    m_avg_packet_vqueue_latency =
        m_packet_queueing_latency / m_packets_received;

    m_avg_packet_network_latency
        .name(name() + ".average_packet_network_latency");
    m_avg_packet_network_latency =
        sum(m_packet_network_latency) / sum(m_packets_received);

    m_avg_packet_queueing_latency
        .name(name() + ".average_packet_queueing_latency");
    m_avg_packet_queueing_latency
        = sum(m_packet_queueing_latency) / sum(m_packets_received);

    m_avg_packet_latency
        .name(name() + ".average_packet_latency");
    m_avg_packet_latency
        = m_avg_packet_network_latency + m_avg_packet_queueing_latency;

    m_avg_marked_pkt_network_latency
        .name(name() + ".average_marked_pkt_network_latency");
    m_avg_marked_pkt_network_latency =
        sum(m_marked_pkt_network_latency) / sum(m_marked_pkt_received);

    m_avg_marked_pkt_queueing_latency
        .name(name() + ".average_marked_pkt_queueing_latency");
    m_avg_marked_pkt_queueing_latency =
        sum(m_marked_pkt_queueing_latency) / sum(m_marked_pkt_received);

    m_avg_marked_pkt_latency
        .name(name() + ".average_marked_pkt_latency");
    m_avg_marked_pkt_latency
        = m_avg_marked_pkt_network_latency + m_avg_marked_pkt_queueing_latency;



    // Flits
    m_flits_received
        .init(m_virtual_networks)
        .name(name() + ".flits_received")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flits_injected
        .init(m_virtual_networks)
        .name(name() + ".flits_injected")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flit_network_latency
        .init(m_virtual_networks)
        .name(name() + ".flit_network_latency")
        .flags(Stats::oneline)
        ;

    m_flit_queueing_latency
        .init(m_virtual_networks)
        .name(name() + ".flit_queueing_latency")
        .flags(Stats::oneline)
        ;

    m_marked_flt_injected
        .init(m_virtual_networks)
        .name(name() + ".marked_flt_injected")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
    m_marked_flt_received
        .init(m_virtual_networks)
        .name(name() + ".marked_flt_received")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
    m_marked_flt_network_latency
        .init(m_virtual_networks)
        .name(name() + ".marked_flt_network_latency")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
    m_marked_flt_queueing_latency
        .init(m_virtual_networks)
        .name(name() + ".marked_flt_queueing_latency")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;


    for (int i = 0; i < m_virtual_networks; i++) {
        m_flits_received.subname(i, csprintf("vnet-%i", i));
        m_flits_injected.subname(i, csprintf("vnet-%i", i));
        m_marked_flt_received.subname(i, csprintf("vnet-%i", i));
        m_marked_flt_injected.subname(i, csprintf("vnet-%i", i));
        m_flit_network_latency.subname(i, csprintf("vnet-%i", i));
        m_flit_queueing_latency.subname(i, csprintf("vnet-%i", i));
        m_marked_flt_network_latency.subname(i, csprintf("vnet-%i", i));
        m_marked_flt_queueing_latency.subname(i, csprintf("vnet-%i", i));
    }

    m_avg_flit_vnet_latency
        .name(name() + ".average_flit_vnet_latency")
        .flags(Stats::oneline);
    m_avg_flit_vnet_latency = m_flit_network_latency / m_flits_received;

    m_avg_flit_vqueue_latency
        .name(name() + ".average_flit_vqueue_latency")
        .flags(Stats::oneline);
    m_avg_flit_vqueue_latency =
        m_flit_queueing_latency / m_flits_received;

    m_avg_flit_network_latency
        .name(name() + ".average_flit_network_latency");
    m_avg_flit_network_latency =
        sum(m_flit_network_latency) / sum(m_flits_received);

    m_avg_flit_queueing_latency
        .name(name() + ".average_flit_queueing_latency");
    m_avg_flit_queueing_latency =
        sum(m_flit_queueing_latency) / sum(m_flits_received);

    m_avg_flit_latency
        .name(name() + ".average_flit_latency");
    m_avg_flit_latency =
        m_avg_flit_network_latency + m_avg_flit_queueing_latency;

    m_avg_marked_flt_network_latency
        .name(name() + ".average_marked_flt_network_latency");
    m_avg_marked_flt_network_latency =
        sum(m_marked_flt_network_latency) / sum(m_marked_flt_received);

    m_avg_marked_flt_queueing_latency
        .name(name() + ".average_marked_flt_queueing_latency");
    m_avg_marked_flt_queueing_latency =
        sum(m_marked_flt_queueing_latency) / sum(m_marked_flt_received);

    m_avg_marked_flt_latency
        .name(name() + ".average_marked_flt_latency");
    m_avg_marked_flt_latency
        = m_avg_marked_flt_network_latency + m_avg_marked_flt_queueing_latency;


    // Hops
    m_avg_hops.name(name() + ".average_hops");
    m_avg_hops = m_total_hops / sum(m_flits_received);

    m_marked_avg_hops.name(name() + ".marked_average_hops");
    m_marked_avg_hops = m_marked_total_hops / sum(m_marked_flt_received);

    // Links
    m_total_ext_in_link_utilization
        .name(name() + ".ext_in_link_utilization");
    m_total_ext_out_link_utilization
        .name(name() + ".ext_out_link_utilization");
    m_total_int_link_utilization
        .name(name() + ".int_link_utilization");
    m_average_link_utilization
        .name(name() + ".avg_link_utilization");

    m_average_vc_load
        .init(m_virtual_networks * m_vcs_per_vnet)
        .name(name() + ".avg_vc_load")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
}

void
GarnetNetwork::collateStats()
{
    RubySystem *rs = params()->ruby_system;
    double time_delta = double(curCycle() - rs->getStartCycle());

    for (int i = 0; i < m_networklinks.size(); i++) {
        link_type type = m_networklinks[i]->getType();
        int activity = m_networklinks[i]->getLinkUtilization();

        if (type == EXT_IN_)
            m_total_ext_in_link_utilization += activity;
        else if (type == EXT_OUT_)
            m_total_ext_out_link_utilization += activity;
        else if (type == INT_)
            m_total_int_link_utilization += activity;

        m_average_link_utilization +=
            (double(activity) / time_delta);

        vector<unsigned int> vc_load = m_networklinks[i]->getVcLoad();
        for (int j = 0; j < vc_load.size(); j++) {
            m_average_vc_load[j] += ((double)vc_load[j] / time_delta);
        }
    }

    // Ask the routers to collate their statistics
    for (int i = 0; i < m_routers.size(); i++) {
        m_routers[i]->collateStats();
    }
}

void
GarnetNetwork::print(ostream& out) const
{
    out << "[GarnetNetwork]";
}

GarnetNetwork *
GarnetNetworkParams::create()
{
    return new GarnetNetwork(this);
}

/*
 * The Garnet Network has an array of routers. These routers have buffers
 * that need to be accessed for functional reads and writes. Also the links
 * between different routers have buffers that need to be accessed.
*/
bool
GarnetNetwork::functionalRead(Packet * pkt)
{
    for(unsigned int i = 0; i < m_routers.size(); i++) {
        if (m_routers[i]->functionalRead(pkt)) {
            return true;
        }
    }

    return false;

}

uint32_t
GarnetNetwork::functionalWrite(Packet *pkt)
{
    uint32_t num_functional_writes = 0;

    for (unsigned int i = 0; i < m_routers.size(); i++) {
        num_functional_writes += m_routers[i]->functionalWrite(pkt);
    }

    for (unsigned int i = 0; i < m_nis.size(); ++i) {
        num_functional_writes += m_nis[i]->functionalWrite(pkt);
    }

    for (unsigned int i = 0; i < m_networklinks.size(); ++i) {
        num_functional_writes += m_networklinks[i]->functionalWrite(pkt);
    }

    return num_functional_writes;
}
