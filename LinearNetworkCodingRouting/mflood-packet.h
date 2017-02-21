#ifndef __mflood_packet_h__
#define __mflood_packet_h__

#include <packet.h>
#include <MFlood/mflood.h>

#define ENCODE  	1
#define ACK		2

//JFlood Routing Protocol Header Macros 
#define HDR_MFLOOD(p)		((struct hdr_mflood*)hdr_mflood::access(p))
#define HDR_MFLOOD_ENCODE(p)	((struct hdr_mflood_encode*)hdr_mflood::access(p)) 

//General JFlood Header
struct hdr_mflood {
	u_int8_t	type;
	u_int32_t	seq_;

	int		batch_ID;
	int		num_decode_pkt;
	int		code_vec[PKT_IN_BATCH];
	int		packet[PKT_IN_BATCH];
	int		encoded_data;

	inline void init() {
           type = 0;
           batch_ID = -1;
	   encoded_data = 0;
	   num_decode_pkt = 0;
           for(int i = 0; i < PKT_IN_BATCH; i++) {
		packet[i] = -1;
		code_vec[i] = -1;
	   }
	}
	
	inline int size() { 
	int sz = 0;
	sz = 3*sizeof(u_int32_t) + (PKT_IN_BATCH + 1)*sizeof(u_int8_t);
  	assert (sz >= 0);
	return sz;
  	}

	// Header access methods
	static int offset_; // required by PacketHeaderManager
	inline static int& offset() { return offset_; }
	inline static hdr_mflood* access(const Packet* p) {
		return (hdr_mflood*) p->access(offset_);
	}
};


/*struct hdr_mflood_encode {
	u_int8_t	em_type;
	int		batch_ID;
	int		code_vec[PKT_IN_BATCH];
	int		data[PKT_IN_BATCH];
	int		encoded_data;

        inline void init() {
           em_type = 0;
           batch_ID = -1;
	   encoded_data = -1;
           for(int i = 0; i < PKT_IN_BATCH; i++) {
		data[i] = -1;
		code_vec[i] = -1;
	   }
	}

	inline int size() { 
	int sz = 0;
	sz = 3*sizeof(u_int32_t) + (PKT_IN_BATCH + 1)*sizeof(u_int8_t);
  	assert (sz >= 0);
	return sz;
  	}	
};

union hdr_all_mflood {
  hdr_mflood          fh;
  hdr_mflood_encode   fh_ech;
};*/

#endif /* __mflood_packet_h__ */
