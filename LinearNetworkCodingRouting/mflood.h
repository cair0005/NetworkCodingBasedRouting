// PBO ================================================================
// I have added some lines into ~ns2/tcl/lib/ns-lib.tcl
// I copy the file ns-lib.tcl to this directory for reference
// PBO ================================================================

#ifndef __mflood_h__
#define __mflood_h__

#include <math.h>
#include <malloc.h>
#include <iostream> 
#include <sys/types.h>
#include <cmu-trace.h>
#include <priqueue.h>
#include <MFlood/mflood-seqtable.h>
#include <list>

#define NOW (Scheduler::instance().clock())

// Should be set by the user using best guess (conservative)
#define NETWORK_DIAMETER 30	// 30 hops
 
// The followings are used for the forward() function. Controls pacing.
#define FORWARD_DELAY 0.01		// random delay
#define NO_DELAY -1.0			// no delay 
#define ONE_SECOND_INTERVAL 1.0
#define PKT_IN_BATCH 3
#define NO_OF_NODES 50
#define BATCH_TTL_INIT 3


struct Gen_Batch {
    //int f;
    //int r;
    //int ID;
    int TTL;   
    int pkt[PKT_IN_BATCH];
};

struct Code_Set {
   int vector[PKT_IN_BATCH];
};

struct Rec_Batch {
    int ID;
    //int seqno;
    int set_count;
    int ack;
    int f;
    int r;
    int code_data[PKT_IN_BATCH];
    int plain_data[PKT_IN_BATCH];
    struct Code_Set code_set[PKT_IN_BATCH];
};

class MFlood;
class BatchTimer : public TimerHandler {
public:
  BatchTimer(MFlood *a) : TimerHandler() {
     a_ = a;
  }
  void expire(Event *e);
protected:
  MFlood *a_;
};

// The Routing Agent
class MFlood: public Agent {
	friend class MFlood_RTEntry;
	friend class BatchTimer;

  public:
	MFlood(nsaddr_t id);
	void recv(Packet *p, Handler *); 
	//Added by CRJ
        bool full_queue(int *front, int *end, int size);
        void enqueue(int *front, int *end, int size);
	void dequeue(int *front, int *end, int size);    

  protected:
	int command(int, const char *const *);
	inline int initialized() { return 1 && target_; }

	// Route Table Management
	void rt_resolve(Packet *p);

	// Packet TX Routines
	//void forward(MFlood_RTEntry *rt, Packet *p, double delay);

	//added by CRJ
	BatchTimer batch_timer;
	void src_get_batch_info(int index);
	void forward_encoded_pkt(Packet *p, double delay);
	void forward(Packet *p, double delay);
	void update_rec_batch_info(Packet *p);
	void clear_rec_batch_info(int index);
	void update_gen_batch_time();
	void send_ACK_msg(int dest, int batchID, int correct_decode_pkt);
	void init_decode_matrix(int index);
	int  decoding_batch(int index);
	bool Gauss(float A[][PKT_IN_BATCH], float B[][PKT_IN_BATCH], int n); 
	bool new_encoding_info(Packet *p, int index);

	nsaddr_t node_id;                  // IP Address of this node

	// Routing Table
	MFlood_RTable rtable_;

	// A mechanism for logging the contents of the routing
	Trace *logtarget;
	NsObject *uptarget_;
	NsObject *port_dmux_;

  private:
  	u_int32_t myseq_;
	int gen_batch_ID;
	float encode_a[PKT_IN_BATCH][PKT_IN_BATCH];
	float inverse_a[PKT_IN_BATCH][PKT_IN_BATCH];
	float encode_data[PKT_IN_BATCH];
	int decode_data[PKT_IN_BATCH];
        struct Gen_Batch gen_batch[NO_OF_NODES];
	struct Rec_Batch rec_batch[NO_OF_NODES];	 
};

#endif 

