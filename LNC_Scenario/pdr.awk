BEGIN {
        sendLine = 0;
	recvLine = 0;
	fowardLine = 0;
  }
   
  {
             event = $1;
             time = $2;
             node_id = $3;
             level = $4;
             reason = $5;
             pkt_type = $7;
             pkt_size = $8;
             
   
  # Store generated packet No.
  if (level == "AGT" && event == "s" ) {
        sendLine ++ ;
  }
 
  # Update total received packets
  if (level == "AGT" && event == "r" ) {
        recvLine ++ ;
  }


  }

END { 
     printf "%.4f s: %d r: %d\n", (recvLine/sendLine), sendLine, recvLine  
     #printf "%d %.4f s: %d r: %d\n", scr, (recvLine/sendLine), sendLine, recvLine >> outfile;    
     #nawk -f pdr.awk mflood-scene2.tr
}
