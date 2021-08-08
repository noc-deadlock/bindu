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


#ifndef __MEM_RUBY_NETWORK_GARNET2_0_GARNETNETWORK_HH__
#define __MEM_RUBY_NETWORK_GARNET2_0_GARNETNETWORK_HH__

#include <iostream>
#include <vector>

#include "mem/ruby/network/Network.hh"
#include "mem/ruby/network/fault_model/FaultModel.hh"
#include "mem/ruby/network/garnet2.0/CommonTypes.hh"
#include "params/GarnetNetwork.hh"
#include "sim/sim_exit.hh"

using namespace std;

class FaultModel;
class NetworkInterface;
class Router;
class NetDest;
class NetworkLink;
class CreditLink;

using namespace std;
class GarnetNetwork : public Network
{
  public:
    typedef GarnetNetworkParams Params;
    GarnetNetwork(const Params *p);

    ~GarnetNetwork();
    void init();

    // Configuration (set externally)

    // for 2D topology
    int getNumRows() const { return m_num_rows; }
    int getNumCols() { return m_num_cols; }

    // for network
    uint32_t getNiFlitSize() const { return m_ni_flit_size; }
    uint32_t getVCsPerVnet() const { return m_vcs_per_vnet; }
    uint32_t getBuffersPerDataVC() { return m_buffers_per_data_vc; }
    uint32_t getBuffersPerCtrlVC() { return m_buffers_per_ctrl_vc; }
    int getRoutingAlgorithm() const { return m_routing_algorithm; }

    bool isFaultModelEnabled() const { return m_enable_fault_model; }
    FaultModel* fault_model;


    // Internal configuration
    bool isVNetOrdered(int vnet) const { return m_ordered[vnet]; }
    VNET_type
    get_vnet_type(int vc)
    {
        int vnet = vc/getVCsPerVnet();
        return m_vnet_type[vnet];
    }
    int getNumRouters();
    std::vector<Router *> get_routers_ref() { return(m_routers); }
    int get_router_id(int ni);


    // Methods used by Topology to setup the network
    void makeExtOutLink(SwitchID src, NodeID dest, BasicLink* link,
                     const NetDest& routing_table_entry);
    void makeExtInLink(NodeID src, SwitchID dest, BasicLink* link,
                    const NetDest& routing_table_entry);
    void makeInternalLink(SwitchID src, SwitchID dest, BasicLink* link,
                          const NetDest& routing_table_entry,
                          PortDirection src_outport_dirn,
                          PortDirection dest_inport_dirn);

    bool functionalRead(Packet * pkt);
    //! Function for performing a functional write. The return value
    //! indicates the number of messages that were written.
    uint32_t functionalWrite(Packet *pkt);

    // Stats
    void collateStats();
    void regStats();
    void print(std::ostream& out) const;

    bool check_mrkd_flt(void);

    // increment counters
    void update_flit_latency_histogram(Cycles& latency, int vnet,
                                                    bool marked) {
        if(marked == true) {
            m_marked_flt_latency_hist.sample(latency);
            if(latency > marked_flit_latency)
                marked_flit_latency = latency;
        }
        else {
            m_flt_latency_hist.sample(latency);
            if(latency > flit_latency)
                flit_latency = latency;
        }
    }

    void update_flit_network_latency_histogram(Cycles& latency,
                                                int vnet, bool marked) {
        if(marked == true) {
            m_marked_flt_network_latency_hist.sample(latency);
            if(latency > marked_flit_network_latency)
                marked_flit_network_latency = latency;
        }
        else {
            m_flt_network_latency_hist.sample(latency);
            if(latency > flit_network_latency)
                flit_network_latency = latency;
        }
    }

    void update_flit_queueing_latency_histogram(Cycles& latency,
                                                int vnet, bool marked) {
        if(marked == true) {
            m_marked_flt_queueing_latency_hist.sample(latency);
            if(latency > marked_flit_queueing_latency)
                marked_flit_queueing_latency = latency;
        }
        else {
            m_flt_queueing_latency_hist.sample(latency);
            if(latency > flit_queueing_latency)
                flit_queueing_latency = latency;
        }
    }

    void increment_injected_packets(int vnet, bool marked) {
        if(marked == true) {
            m_marked_pkt_injected[vnet]++;
        }

        m_packets_injected[vnet]++;
    }
    void increment_received_packets(int vnet, bool marked) {
        if(marked == true) {
            m_marked_pkt_received[vnet]++;
        }
        m_packets_received[vnet]++;
		/*
        cout << "m_total_packets_received: " << m_total_packets_received++ \
           << " at cycle: "<< curCycle() << endl;
		*/
    }

    Router*
    get_RouterInDirn(PortDirection outport_dir, int upstream_id);


    void
    increment_packet_network_latency(Cycles latency, int vnet, bool marked)
    {
        m_packet_network_latency[vnet] += latency;
        if(marked == true) {
            m_marked_pkt_network_latency[vnet] += latency;
        }
    }

    void
    increment_packet_queueing_latency(Cycles latency, int vnet, bool marked)
     {
        m_packet_queueing_latency[vnet] += latency;
        if(marked == true) {
            m_marked_pkt_queueing_latency[vnet] += latency;
        }
    }

    void increment_injected_flits(int vnet, bool marked, int m_router_id) {
          m_flits_injected[vnet]++;
          // std::cout << "flit injected into the network..." << std::endl;
          m_flt_dist[m_router_id]++;
          if(marked == true) {
              m_marked_flt_injected[vnet]++;
              m_marked_flt_dist[m_router_id]++;
              marked_flt_injected++;
              std::cout << "marked flit injected: " << marked_flt_injected \
                << " at cycle(): " << curCycle() << std::endl;
          }
        //      if (curCycle() > 550) {
        //            scanNetwork();
        //            assert(0);
        //      }
        }


    void increment_received_flits(int vnet, bool marked) {
        m_flits_received[vnet]++;
        if(marked == true) {
            m_marked_flt_received[vnet]++;
            marked_flt_received++;
            total_marked_flit_received++;

            bool sim_exit;
            sim_exit = check_mrkd_flt();

            if(sim_exit) {
                cout << "marked_flt_injected: " << marked_flt_injected << endl;
                cout << "marked_flt_received: " << marked_flt_received << endl;
                assert(marked_flt_injected == marked_flt_received);
                cout << "marked_flits: " << marked_flits << endl;
                // transfer all numbers to stat variable:
                  m_max_flit_latency = flit_latency;
                  m_max_flit_network_latency = flit_network_latency;
                  m_max_flit_queueing_latency = flit_queueing_latency;
                  m_max_marked_flit_latency = marked_flit_latency;
                  m_max_marked_flit_network_latency = marked_flit_network_latency;
                  m_max_marked_flit_queueing_latency = marked_flit_queueing_latency;

                exitSimLoop("All marked packet received.");
            }

        }
    }

    void
    increment_flit_network_latency(Cycles latency, int vnet, bool marked)
    {
        m_flit_network_latency[vnet] += latency;
        if(marked == true) {
            m_marked_flt_network_latency[vnet] += latency;
            total_marked_flit_latency += (uint64_t)latency;
        }
    }

    void
    increment_flit_queueing_latency(Cycles latency, int vnet, bool marked)
    {
        m_flit_queueing_latency[vnet] += latency;
        if(marked == true) {
            m_marked_flt_queueing_latency[vnet] += latency;
            total_marked_flit_latency += (uint64_t)latency;
        }
    }

    void
    increment_total_hops(int hops, bool marked)
    {
        m_total_hops += hops;
        if(marked == true) {
            m_marked_total_hops += hops;
        }
    }

    void
    check_network_saturation()
    {
        double avg_flt_network_latency;
        /*cout << "total marked_flit latency (queuing+network): " \
            << total_marked_flit_latency << std::endl;*/
        cout << "total marked flit latency: " << total_marked_flit_latency << endl;
        cout << "total marked flit received: " << total_marked_flit_received << endl;
        if (total_marked_flit_received > 0) {
            avg_flt_network_latency =
                (double)total_marked_flit_latency/(double)total_marked_flit_received;
            cout << "average marked flit latency: " << avg_flt_network_latency << endl;
            cout.flush();
        }
        else {
            avg_flt_network_latency = 0.0;
        }
        if(avg_flt_network_latency > 1000.0)
            exitSimLoop("avg flit latency exceeded threshold!.");
        // Due to livelock if sim-type-2 takes a very long time
        // then exit thsi simulation so that other can procced.
        if(curCycle() > 10000000 ) {
            m_pre_mature_exit++;
            exitSimLoop("Simulation exceed its cycle quota!");
        }
    }


    void print_brownian_bubbles();
    void init_brownian_bubbles();
    void init_brownian_bubbles_cbs();
    void set_bubble_inport(int k, string dirn_);
    void print_topology();
    bool move_intra_bubble(int bubble_id);
    bool move_inter_bubble(int bubble_id);
    bool move_inter_bubble_cbs(int bubble_id);
    int move_next_router(int curr_router_id, int bubble_id );
    int move_next_router_cbs(int curr_router_id, int bubble_id);


    uint32_t m_enable_bn;
    uint32_t m_num_bubble;
    bool m_bubble_init;
    uint32_t m_intra_bubble_period;
    uint32_t m_inter_bubble_period;
    Cycles last_inter_bubble_movement;
    Cycles last_intra_bubble_movement;
    uint64_t m_total_packets_received;
    string m_scheme;

    struct brownian_bubble {
        uint32_t bubble_id;
        uint32_t router_id;
        // uint32_t vc_id; // one bb per vnet, can be identified from vc_id
        uint32_t inport_id;
        uint32_t last_inport_id; // this is because bubble move in rr fashion
        PortDirection inport_dirn;
        PortDirection last_inport_dirn;
        // these variables are used to decide if bubble should move or not
        // and what kind of movement is needed.
        Cycles last_intra_movement_cycle;
        Cycles last_inter_movement_cycle;
        bool sense_inc; // dictates the sense of movement of bubbles
    };

    std::vector<brownian_bubble> bubble;

    uint64_t total_marked_flit_latency;
    uint64_t total_marked_flit_received;

    Stats::Scalar m_pre_mature_exit;

    uint64_t marked_flt_injected;
    uint64_t marked_flt_received;
    uint64_t marked_pkt_injected;
    uint64_t marked_pkt_received;
    uint64_t warmup_cycles;
    uint64_t marked_flits;

    int sim_type;
    Cycles flit_latency;
    Cycles flit_network_latency;
    Cycles flit_queueing_latency;
    Cycles marked_flit_latency;
    Cycles marked_flit_network_latency;
    Cycles marked_flit_queueing_latency;

    int rand_bb;

  protected:
    Stats::Vector m_marked_flt_dist;
    Stats::Vector m_flt_dist;

    // Configuration
    int m_num_rows;
    int m_num_cols;
    uint32_t m_ni_flit_size;
    uint32_t m_vcs_per_vnet;
    uint32_t m_buffers_per_ctrl_vc;
    uint32_t m_buffers_per_data_vc;
    int m_routing_algorithm;
    bool m_enable_fault_model;

    // Statistical variables

    // statistical variable
    // Brownian Network Related
    Stats::Scalar m_num_inter_swap;
    Stats::Scalar m_num_intra_swap;
    Stats::Scalar m_inter_swap_bubble;
    Stats::Scalar m_intra_swap_bubble;
    Stats::Scalar m_intra_swap_pkt;
    Stats::Scalar m_inter_swap_pkt;

    Stats::Scalar m_max_flit_latency;
    Stats::Scalar m_max_flit_network_latency;
    Stats::Scalar m_max_flit_queueing_latency;
    Stats::Scalar m_max_marked_flit_latency;
    Stats::Scalar m_max_marked_flit_network_latency;
    Stats::Scalar m_max_marked_flit_queueing_latency;


    //! Histogram !//
    Stats::Histogram m_flt_latency_hist;
    Stats::Histogram m_marked_flt_latency_hist;
    Stats::Histogram m_flt_network_latency_hist;
    Stats::Histogram m_flt_queueing_latency_hist;
    Stats::Histogram m_marked_flt_network_latency_hist;
    Stats::Histogram m_marked_flt_queueing_latency_hist;

    Stats::Vector m_network_latency_histogram;
    Stats::Vector m_packets_received;
    Stats::Vector m_packets_injected;
    Stats::Vector m_packet_network_latency;
    Stats::Vector m_packet_queueing_latency;

    Stats::Vector m_marked_pkt_network_latency;
    Stats::Vector m_marked_pkt_queueing_latency;
    Stats::Vector m_marked_flt_network_latency;
    Stats::Vector m_marked_flt_queueing_latency;

    Stats::Vector m_marked_flt_injected;
    Stats::Vector m_marked_flt_received;
    Stats::Vector m_marked_pkt_injected;
    Stats::Vector m_marked_pkt_received;

    Stats::Formula m_avg_marked_flt_latency;
    Stats::Formula m_avg_marked_pkt_latency;
    Stats::Formula m_avg_marked_pkt_network_latency;
    Stats::Formula m_avg_marked_pkt_queueing_latency;
    Stats::Formula m_avg_marked_flt_network_latency;
    Stats::Formula m_avg_marked_flt_queueing_latency;
    Stats::Formula m_marked_avg_hops;
    Stats::Scalar m_marked_total_hops;

    Stats::Formula m_avg_packet_vnet_latency;
    Stats::Formula m_avg_packet_vqueue_latency;
    Stats::Formula m_avg_packet_network_latency;
    Stats::Formula m_avg_packet_queueing_latency;
    Stats::Formula m_avg_packet_latency;

    Stats::Vector m_flits_received;
    Stats::Vector m_flits_injected;
    Stats::Vector m_flit_network_latency;
    Stats::Vector m_flit_queueing_latency;

    Stats::Formula m_avg_flit_vnet_latency;
    Stats::Formula m_avg_flit_vqueue_latency;
    Stats::Formula m_avg_flit_network_latency;
    Stats::Formula m_avg_flit_queueing_latency;
    Stats::Formula m_avg_flit_latency;

    Stats::Scalar m_total_ext_in_link_utilization;
    Stats::Scalar m_total_ext_out_link_utilization;
    Stats::Scalar m_total_int_link_utilization;
    Stats::Scalar m_average_link_utilization;
    Stats::Vector m_average_vc_load;

    Stats::Scalar  m_total_hops;
    Stats::Formula m_avg_hops;

  private:
    GarnetNetwork(const GarnetNetwork& obj);
    GarnetNetwork& operator=(const GarnetNetwork& obj);

    std::vector<VNET_type > m_vnet_type;
    std::vector<Router *> m_routers;   // All Routers in Network
    std::vector<NetworkLink *> m_networklinks; // All flit links in the network
    std::vector<CreditLink *> m_creditlinks; // All credit links in the network
    std::vector<NetworkInterface *> m_nis;   // All NI's in Network
};

inline std::ostream&
operator<<(std::ostream& out, const GarnetNetwork& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

#endif //__MEM_RUBY_NETWORK_GARNET2_0_GARNETNETWORK_HH__
