#include <MFlood/mflood.h>
#include <MFlood/mflood-packet.h>
#include <random.h>
#include <cmu-trace.h>


// bind New packet to Tcl interface
int hdr_mflood::offset_;
static class MFloodHeaderClass : public PacketHeaderClass {
public:
	MFloodHeaderClass() : PacketHeaderClass("PacketHeader/MFlood", 
					      sizeof(hdr_mflood)) {
		bind_offset(&hdr_mflood::offset_);
	}
} class_mfloodhdr;


// TCL Hooks
static class MFloodclass : public TclClass {
public:
	MFloodclass() : TclClass("Agent/MFlood") {}
	TclObject* create(int argc, const char*const* argv) {
		assert(argc == 5);
		return (new MFlood((nsaddr_t)Address::instance().str2addr(argv[4])));
		//return (new MFlood((nsaddr_t) atoi(argv[4])));	// PBO agrv[4] is node_id}
	}
} class_rtProtoMFlood;

void
BatchTimer::expire(Event *) {
  a_->update_gen_batch_time();
  resched(ONE_SECOND_INTERVAL);
}

void
MFlood::update_gen_batch_time() {
  for(int i = 0; i < NO_OF_NODES; i++)
  {
    if(gen_batch[i].TTL > 0)
     gen_batch[i].TTL--;
    if(gen_batch[i].TTL == 0)
    {
      gen_batch[i].TTL = -1;
      //batch[i].f = -1;
      //batch[i].r = -1;
      for(int j = 0; j < PKT_IN_BATCH; j++)
	gen_batch[i].pkt[j] = -1;
    }
  }
}

int
MFlood::command(int argc, const char*const* argv) {
	Tcl& tcl = Tcl::instance();
	if(argc == 2) {		
		if(strncasecmp(argv[1], "id", 2) == 0) {
			tcl.resultf("%d", node_id);
			return TCL_OK;
		}
		else if (strcmp(argv[1], "uptarget") == 0) {
			if (uptarget_ != 0)
				tcl.result(uptarget_->name());
			return (TCL_OK);
		}
		else if (strncasecmp(argv[1], "start", 2) == 0) {
			batch_timer.resched(0.1);
			return (TCL_OK);
		}
	} else if(argc == 3) {
		if(strcmp(argv[1], "node_id") == 0) {
			node_id = atoi(argv[2]);
			return TCL_OK;
		} else if(strcmp(argv[1], "log-target") == 0 || strcmp(argv[1], "tracetarget") == 0) {
			logtarget = (Trace*) TclObject::lookup(argv[2]);
			if(logtarget == 0) return TCL_ERROR;
			return TCL_OK;
		}
		else if (strcmp(argv[1], "uptarget") == 0) {
			if (*argv[2] == '0') {
				target_ = 0;
				return (TCL_OK);
			}
			uptarget_ = (NsObject*)TclObject::lookup(argv[2]);
			if (uptarget_ == 0) {
				tcl.resultf("no such object %s", argv[2]);
				return (TCL_ERROR);
			}
			return (TCL_OK);
		}
	else if (strcasecmp (argv[1], "port-dmux") == 0) {
	TclObject *obj;
	port_dmux_ = (NsObject *) obj;
	return TCL_OK;
	}		
}
	return Agent::command(argc, argv);
}

MFlood::MFlood(nsaddr_t id) : Agent(PT_MFLOOD), batch_timer(this) {
	for(int i = 0; i < NO_OF_NODES; i++) {
	  rec_batch[i].set_count = 0;
	  rec_batch[i].ID = -1;
	  rec_batch[i].f = -1;
	  rec_batch[i].r = -1;
	  rec_batch[i].ack = 0;
	  //gen_batch[i].f = -1;
	  //gen_batch[i].r = -1;
	  gen_batch[i].TTL = -1;	
	  for(int j=0; j < PKT_IN_BATCH; j++) {
	    rec_batch[i].code_data[j] = 0;
	    rec_batch[i].plain_data[j] = 0;
	    gen_batch[i].pkt[j] = 0;
	    for(int k=0; k < PKT_IN_BATCH; k++)
	       rec_batch[i].code_set[j].vector[k] = 0;
	  }
	}
	
	gen_batch_ID = 0;
	/*batch.f = -1;	
	batch.r = -1;	
	batch.TTL = -1;
	batch.ID = 0;*/

	node_id = id;
	logtarget = 0;
	myseq_ = 0;
}

bool
MFlood::full_queue(int *front, int *end, int size) {
  if (*front==0 && (*end+1)==size)
   return true;
  else if ((*front-*end)==1)
   return true;
  else
   return false;
}

void
MFlood::enqueue(int *front, int *end, int size) {
   if (*front==-1 && *end == -1)
     *front=*end=0;
   else
   {
     *end=*end+1;
     if (*end==size)
       *end=0;
   }  
}

void
MFlood::dequeue(int *front, int *end, int size) {
  if(*front == *end)
   *front = *end = -1;  
  else
  {
    *front = *front + 1;
    if(*front == size)
     *front = 0;
  }
}

// Route Handling Functions
void
MFlood::rt_resolve(Packet *p) {
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_mflood *fh = HDR_MFLOOD(p);
	MFlood_RTEntry* rt;

	rt = rtable_.rt_lookup(ih->saddr());
	if(rt == NULL) {
		rt = new MFlood_RTEntry(ih->saddr(), fh->seq_);

		LIST_INSERT_HEAD(&rtable_.rthead,rt,rt_link);		
	
		//forward(rt,p,FORWARD_DELAY);
                if(fh->type == ENCODE)
		 forward_encoded_pkt(p,FORWARD_DELAY);		
		else
		 forward(p, 0.0);		

		rtable_.rt_print();				
	}

	else if(rt->isNewSeq(fh->seq_) )
	{		
		//forward(rt, p, FORWARD_DELAY);
		if(fh->type == ENCODE)
		 forward_encoded_pkt(p,FORWARD_DELAY);
		else
		 forward(p, 0.0);		

		rt->addSeq(fh->seq_);
		rtable_.rt_print();		
	}
	else
	{
		drop(p, "LSEQ");
	}
}

void
MFlood::forward(Packet *p, double delay) {
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_mflood *fh = HDR_MFLOOD(p);

	assert(ih->ttl_ > 0);
	assert(rt != 0);

	ch->next_hop_ = -1;			//Broadcast address
	ch->addr_type() = NS_AF_INET;
	ch->direction() = hdr_cmn::DOWN;       //important: change the packet's direction
	ch->prev_hop_ = node_id;
	
 	Scheduler::instance().schedule(target_, p, 0.);	
}


void
MFlood::src_get_batch_info(int index) {
 if(gen_batch[index].TTL == -1)
 {
   gen_batch_ID++;
   gen_batch[index].TTL = BATCH_TTL_INIT;
   for(int i=0; i < PKT_IN_BATCH; i++)
     gen_batch[index].pkt[i] = (Random::random() % 101);
 } 
 else
   return;
}

void
MFlood::forward_encoded_pkt(Packet *p, double delay) {   
    //Packet *p_copy = p->copy();
    struct hdr_cmn *ch = HDR_CMN(p);
    struct hdr_ip *ih = HDR_IP(p); 
    struct hdr_mflood *fh = HDR_MFLOOD(p);

    if((ih->saddr() == node_id) && (ch->num_forwards() == 0))
    {
       src_get_batch_info(ih->daddr());
       fh->init();
       fh->type = ENCODE;
       fh->batch_ID = gen_batch_ID;
       for(int i = 0; i < PKT_IN_BATCH; i++) {
         fh->code_vec[i] = (Random::random() % 256);
         fh->packet[i] = gen_batch[ih->daddr()].pkt[i];
         fh->encoded_data += fh->code_vec[i]*fh->packet[i];
      }
      ch->size()+= fh->size();
    }

    ih->sport() = RT_PORT;
    ih->dport() = RT_PORT;

    //ch->ptype() = PT_MFLOOD;
    ch->addr_type() = NS_AF_INET;
    ch->next_hop_ = -1;	                    //Broadcast address
    ch->prev_hop_ = node_id;    
    ch->direction() = hdr_cmn::DOWN;       //important: change the packet's direction

    if(delay > 0.0) 
    {
 	Scheduler::instance().schedule(target_, p, Random::uniform(delay*2));
    } 
    else 
    {	// Not a broadcast packet, no delay, send immediately
 	Scheduler::instance().schedule(target_, p, 0.);
    }
}

void
MFlood::send_ACK_msg(int dest, int batchID, int correct_decode_pkt) {
	Packet *p = Packet::alloc();
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_mflood *fh = HDR_MFLOOD(p);

	ch->ptype() = PT_MFLOOD;
 	ch->size() = IP_HDR_LEN + 13;
 	ch->addr_type() = NS_AF_INET;
 	ch->prev_hop_ = node_id; 
	ch->next_hop_ = -1;	
	ch->direction() = hdr_cmn::DOWN;

	ih->saddr() = node_id;
 	ih->daddr() = dest;
 	ih->sport() = RT_PORT;
 	ih->dport() = RT_PORT;
	ih->ttl_ = NETWORK_DIAMETER;

	fh->init();
	fh->type = ACK;
	fh->seq_ = myseq_++;
	fh->batch_ID = batchID;
	fh->num_decode_pkt = correct_decode_pkt;
	ch->uid() = fh->seq_;

	sprintf(logtarget->pt_->buffer(), "S %0.9f %d AGT  --- %d ACK %d [13a %d %d 800]   ------- [%d:%d %d:%d %d %d]", NOW, node_id, fh->batch_ID, ch->size(), ch->uid(), correct_decode_pkt, ih->saddr(), ih->sport(), ih->daddr(), ih->dport(), ih->ttl_, (ch->next_hop_ < 0) ? 0 : ch->prev_hop_);
	logtarget->pt_->dump();

  	Scheduler::instance().schedule(target_, p, 0.);
}

bool 
MFlood::Gauss(float A[][PKT_IN_BATCH], float B[][PKT_IN_BATCH], int n) {
    int i, j, k;
    float max, temp;
    float t[PKT_IN_BATCH][PKT_IN_BATCH];                //temporary matrix
    //store matrix A into temporary matrix t
    for (i = 0; i < n; i++)        
    {
        for (j = 0; j < n; j++)
        {
            t[i][j] = A[i][j];
        }
    }
    //initialize matrix B to an identity matrix
    for (i = 0; i < n; i++)        
    {
        for (j = 0; j < n; j++)
        {
            B[i][j] = (i == j) ? (float)1 : 0;
        }
    }
    for (i = 0; i < n; i++)
    {
        max = t[i][i];
        k = i;
        for (j = i+1; j < n; j++)
        {
            if (fabs(t[j][i]) > fabs(max))
            {
                max = t[j][i];
                k = j;
            }
        }
        if (k != i)
        {
            for (j = 0; j < n; j++)
            {
                temp = t[i][j];
                t[i][j] = t[k][j];
                t[k][j] = temp;
                temp = B[i][j];
                B[i][j] = B[k][j];
                B[k][j] = temp; 
            }
        }
        if (t[i][i] == 0)
        {
            printf("There is no inverse matrix!\n");
            return false;
        }
   
        temp = t[i][i];
        for (j = 0; j < n; j++)
        {
            t[i][j] = t[i][j] / temp;        
            B[i][j] = B[i][j] / temp;        
        }
        for (j = 0; j < n; j++)        
        {
            if (j != i)                
            {
                temp = t[j][i];
                for (k = 0; k < n; k++)       
                {
                    t[j][k] = t[j][k] - t[i][k]*temp;
                    B[j][k] = B[j][k] - B[i][k]*temp;
                }
            }
        }
    }	
    return true;
}

void
MFlood::init_decode_matrix(int index) {
  for(int i = 0; i < PKT_IN_BATCH; i++)
  {
    encode_data[i] = rec_batch[index].code_data[i];
    decode_data[i] = 0;
    for(int j = 0; j < PKT_IN_BATCH; j++)
    {
      encode_a[i][j] = rec_batch[index].code_set[i].vector[j];
      inverse_a[i][j] = 0.0;
    } 
  }
}

int
MFlood::decoding_batch(int index) {
  float temp[PKT_IN_BATCH] = {0};
  int num_decode_pkt = 0;
  if (Gauss(encode_a, inverse_a, PKT_IN_BATCH))
  {
    for(int i = 0 ; i < PKT_IN_BATCH; i++)
    {
      for(int j = 0; j < PKT_IN_BATCH; j++)
       temp[i] += inverse_a[i][j] * encode_data[j];
    }
    
    for(int a = 0; a < PKT_IN_BATCH; a++) {
      decode_data[a] = (int)round(temp[a]);
      if(decode_data[a] == rec_batch[index].plain_data[a])
        num_decode_pkt++;
    }
       
   //if(num_decode_pkt != PKT_IN_BATCH)
    //printf("aaaaaaaa\n");
   
   return num_decode_pkt;
  }
  else
    return -1;
}

void
MFlood::clear_rec_batch_info(int index) {
	rec_batch[index].ID = -1;
	//rec_batch[index].seqno = -1;
	rec_batch[index].set_count = 0;	
	rec_batch[index].ack = 0;
	for(int j=0; j < PKT_IN_BATCH; j++) {
	    rec_batch[index].code_data[j] = 0;
	    rec_batch[index].plain_data[j] = 0;
	    for(int k=0; k < PKT_IN_BATCH; k++)
	       rec_batch[index].code_set[j].vector[k] = 0;
	}
	rec_batch[index].f = -1;
	rec_batch[index].r = -1;
}

bool
MFlood::new_encoding_info(Packet *p, int index) {
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_mflood *fh = HDR_MFLOOD(p);

	int i = 0;
	for(; i < PKT_IN_BATCH; i++)
        {
	  if(fh->encoded_data == rec_batch[index].code_data[i])
	     break;   	  
        }

	if(i == PKT_IN_BATCH)
	  return true;
	else
	  return false;
}

void
MFlood::update_rec_batch_info(Packet *p) {
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_mflood *fh = HDR_MFLOOD(p);

	if(rec_batch[ih->saddr()].set_count == 0) 
	{
	  rec_batch[ih->saddr()].ID = fh->batch_ID;
	  enqueue(&rec_batch[ih->saddr()].f, &rec_batch[ih->saddr()].r, PKT_IN_BATCH);
          int index = rec_batch[ih->saddr()].set_count;
	  rec_batch[ih->saddr()].code_data[index] = fh->encoded_data;
	  for(int i = 0; i < PKT_IN_BATCH; i++) {
	    rec_batch[ih->saddr()].code_set[index].vector[i] = fh->code_vec[i];	 
	    rec_batch[ih->saddr()].plain_data[i] = fh->packet[i];
	  }
	  rec_batch[ih->saddr()].set_count++;
	}
	else
        {
	   if(rec_batch[ih->saddr()].ID < fh->batch_ID)
           {
             clear_rec_batch_info(ih->saddr());
	     rec_batch[ih->saddr()].ID = fh->batch_ID;
	     enqueue(&rec_batch[ih->saddr()].f, &rec_batch[ih->saddr()].r, PKT_IN_BATCH);
             int index = rec_batch[ih->saddr()].set_count;
	     rec_batch[ih->saddr()].code_data[index] = fh->encoded_data;
	     for(int j = 0; j < PKT_IN_BATCH; j++) {
	       rec_batch[ih->saddr()].code_set[index].vector[j] = fh->code_vec[j];	       
	       rec_batch[ih->saddr()].plain_data[j] = fh->packet[j];
	     }
	     rec_batch[ih->saddr()].set_count++;
           }
	   else
	   {
             if(new_encoding_info(p, ih->saddr())) 
             { 
		int index = rec_batch[ih->saddr()].set_count; 	     
		if(index > 0 && index < PKT_IN_BATCH)
		{              
	          enqueue(&rec_batch[ih->saddr()].f, &rec_batch[ih->saddr()].r, PKT_IN_BATCH);
                  rec_batch[ih->saddr()].code_data[index] = fh->encoded_data;
                  for(int k = 0; k < PKT_IN_BATCH; k++)
                  {
		    assert(rec_batch[ih->saddr()].plain_data[k] == fh->packet[k]);
 		    rec_batch[ih->saddr()].code_set[index].vector[k] = fh->code_vec[k];
                  }
                    rec_batch[ih->saddr()].set_count++;
                }
		else
	        {
	          enqueue(&rec_batch[ih->saddr()].f, &rec_batch[ih->saddr()].r, PKT_IN_BATCH);
	          int rear = rec_batch[ih->saddr()].r;
	          for(int a = 0; a < PKT_IN_BATCH; a++)
	          {
		    assert(rec_batch[ih->saddr()].plain_data[a] == fh->packet[a]);
		    rec_batch[ih->saddr()].code_set[rear].vector[a] = fh->code_vec[a];
	          }
	        }
              }
	      else
		return;
	   }		
	}
}


// Packet Reception Routines
void
MFlood::recv(Packet *p, Handler*) {
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_mflood *fh = HDR_MFLOOD(p);
	assert(initialized());
	//int success_decode_pkt = 0; 
             
	//if(ch->ptype() == PT_MFLOOD && ih->daddr() == node_id)
	if(ih->daddr() == node_id && fh->type == ENCODE)
	{
           if(rec_batch[ih->saddr()].ack != 1 || rec_batch[ih->saddr()].ID < fh->batch_ID)
           {
              update_rec_batch_info(p);
                                                               
	      if(rec_batch[ih->saddr()].set_count == PKT_IN_BATCH)
	      {		 
		  init_decode_matrix(ih->saddr());
		  int success_decode_pkt = decoding_batch(ih->saddr());
		  if(success_decode_pkt != -1) 
		  {
		    send_ACK_msg(ih->saddr(), rec_batch[ih->saddr()].ID, PKT_IN_BATCH); 
		    rec_batch[ih->saddr()].ack = 1;
		    sprintf(logtarget->pt_->buffer(), "R %0.9f %d AGT  --- %d bat %d [13a %d %d 800]   ------- [%d:%d %d:%d %d %d] Decode", NOW, node_id, rec_batch[ih->saddr()].ID, ch->size(), ch->uid(), success_decode_pkt, ih->saddr(), ih->sport(), ih->daddr(), ih->dport(), ih->ttl_, (ch->next_hop_ < 0) ? 0 : ch->prev_hop_);
	            logtarget->pt_->dump();

                  }
		  else
		    printf("unable to decode pkt\n");			   
	      }

	      ch->size() -= fh->size();	
  	      fh->type = 0;
  	      ch->size() -= IP_HDR_LEN;       
	      ch->direction() = hdr_cmn::DOWN;
  	      target_->recv(p, (Handler*)0);	      
	      p = 0;
           }
           else
	     Packet::free(p);	
	   return;
	}

	if(ih->daddr() == node_id && fh->type == ACK)
        {
	   gen_batch[ih->saddr()].TTL = -1;
	   for(int i = 0; i < PKT_IN_BATCH; i++)
	    gen_batch[ih->saddr()].pkt[i] = 0;

	   sprintf(logtarget->pt_->buffer(), "R %0.9f %d AGT  --- %d ACK %d [13a %d %d 800]   ------- [%d:%d %d:%d %d %d]", NOW, node_id, fh->batch_ID, ch->size(), ch->uid(), fh->num_decode_pkt, ih->saddr(), ih->sport(), ih->daddr(), ih->dport(), ih->ttl_, (ch->next_hop_ < 0) ? 0 : ch->prev_hop_);
	   logtarget->pt_->dump();

	   Packet::free(p);
	   return;
	   //printf("bbbbbbbb\n");
	}

	if((ih->saddr() == node_id) && (ch->num_forwards() == 0)) // Must be a packet I'm orig.
	{		
           	if(ih->daddr() == node_id) {		
		   Packet::free(p);
		   return;
	        }

		ch->size() += IP_HDR_LEN;		// Add the IP Header
		ih->ttl_ = NETWORK_DIAMETER;
		fh->seq_ = myseq_++;
		forward_encoded_pkt(p,0);

		sprintf(logtarget->pt_->buffer(), "S %0.9f %d AGT  --- %d bat %d [13a %d %d 800]   ------- [%d:%d %d:%d %d %d] Encode", NOW, node_id, gen_batch_ID, ch->size(), ch->uid(), PKT_IN_BATCH, ih->saddr(), ih->sport(), ih->daddr(), ih->dport(), ih->ttl_, (ch->next_hop_ < 0) ? 0 : ch->prev_hop_);
                logtarget->pt_->dump();                              		
		return;
	} 
	else if(ih->saddr() == node_id) // I rec a packet that I sent.Probably a routing loop.
	{	
		drop(p, DROP_RTR_ROUTE_LOOP);
		return;
	} 
	else 	// Packet I'm forwarding...
	{		
		if(--ih->ttl_ == 0) 	// Check the TTL.  If it is zero, then discard.
		{	
			drop(p, DROP_RTR_TTL);
	 		return;
		}
	}

	
        printf("current node is %d\n", node_id);
	rt_resolve(p);
}


// Packet Transmission Routines
/*void
MFlood::forward(MFlood_RTEntry* rt, Packet *p, double delay) {
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_mflood *fh = HDR_MFLOOD(p);
	//struct hdr_mflood_encode *emh = HDR_MFLOOD(p);

	assert(ih->ttl_ > 0);
	assert(rt != 0);

  	ch->size()+= fh->size();
	ch->next_hop_ = -1;	//Broadcast address
	ch->addr_type() = NS_AF_INET;
	ch->direction() = hdr_cmn::DOWN;       //important: change the packet's direction
	if(delay > 0.0) {
 		Scheduler::instance().schedule(target_, p, Random::uniform(delay*2));
	} else {		// Not a broadcast packet, no delay, send immediately
 		Scheduler::instance().schedule(target_, p, 0.);
	}
}*/



