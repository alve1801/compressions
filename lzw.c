#include<stdio.h>
#include<stdlib.h> // for malloc, since this is ansi-c and we gotta do memory management ourselves
// lz78 (expanding prefix)

// also, use fixed bit depth output. we can figure out bitpacking once we get this bitch working

void lzw_encode(char*input,char*out){
	// c stores all codes as length-data
	char c[10000];
	for(int i=0,t=1;t<127;t++)c[i++]=1,c[i++]=t;

	while(*input){
		int current=0,lmatch,lmlen=0; // index of current code, index of longest code so far, and its length
		char*code=c,*ldata,match,clen; // current position in data, and position of longest code - so we can add a new entry to data
		while(*code){
			match=1; // bool telling us whether the current code is correct
			clen=*code++; // length of current code

			if(clen>lmlen){ // no need to bother otherwise
				for(int i=0;i<clen;i++)
					if(code[i]!=input[i])
						match=0;

				if(match)
					lmatch=current,
					ldata=code,
					lmlen=clen;
					// no break bc there might be a longer code later on
			}
			code+=clen;current++;
		}

		out[0]=lmatch;out++;
		input+=lmlen;
		// add new code
		*code++=lmlen+1;
		for(int i=0;i<lmlen;i++)code[i]=ldata[i]; // *code++=*ldata++;
		code[lmlen]=*input;
	}

	return;

	int i=0;
	for(char*at=c,l;*at;i++){
		l=*at++;
		//printf("code %3i of length %i:",i,l);
		printf("<%i:",i);
		while(l--)putchar(*at++);
		putchar('>');putchar(32);
		//putchar(10);
	}putchar(10);
}

void lzw_decode(char*input){
	// similar to encode, but here we have an extra table for index lookup
	char data[10000],prev=*input++;int codes[4096],max=1;
	for(int i=0;max<127;max++){
		data[i++]=1;
		data[i++]=max;
		codes[max]=i; // wait, why AFTER?
	}codes[max]=(max<<1)-2;data[codes[max]]=0;
	max--;

	putchar(data[codes[prev]+1]);

	// XXX could put the appending of new code at beginning/end - iow separate it from the case checker

	while(*input){
		int at=*input;
		if(at<max){
			for(int i=1;i<data[codes[at]]+1;i++)
				putchar(data[codes[at]+i]);
			//fflush(0); // so at least debug works

			// add previous plus first of current
			// okay wow this is a mess
			data[codes[max]]=data[codes[prev]]+1;
			for(int i=1;i<data[codes[prev]]+1;i++)
				data[codes[max]+i]=data[codes[prev]+i];
			data[codes[max]+data[codes[max]]]=data[codes[at]+1];
			codes[max+1]=codes[max]+data[codes[max]]+1;
			prev=at;

		}else{
			// take prev
			// append to it its first char
			// output that and store it as new code

			// same as above, but append a different character
			data[codes[max]]=data[codes[prev]]+1;
			for(int i=1;i<data[codes[prev]]+1;i++)
				data[codes[max]+i]=data[codes[prev]+i];
			data[codes[max]+data[codes[max]]]=data[codes[max]+1];
			codes[max+1]=codes[max]+data[codes[max]]+1;
			prev=max;

			for(int i=1;i<data[codes[max]]+1;i++)
				putchar(data[codes[max]+i]);
		}
		input++;max++;
	}
	putchar(10);
	return;

	/*int i=0; // XXX this should ideally also output the mapping in 'codes'
	for(char*at=data,l;*at;i++){
		l=*at++;
		//printf("code %3i of length %i:",i,l);
		printf("<%i:",i);
		while(l--)putchar(*at++);
		putchar('>');putchar(32);
		//putchar(10);
	}putchar(10);
	*/

	for(int i=2;codes[i];i++){
		printf("<%i:",i);
		char l=data[codes[i]];
		for(int l=1;l<data[codes[i]]+1;l++)
			putchar(data[codes[i]+l]);
		putchar('>');putchar(32);
	}putchar(10);
}



// lz77 (aka deflate)

// no huffman, no bit packing - iow we are only interested in the backreference so far
// oh, and no blocking either - we are not gonna need that
// an opcode has the highest bit set to 1 (since we only encode ascii), followed by length, followed by one byte of offset
//  iow it also doesnt make sense to encode matches shorter than three bytes
// makes sense to have different prefixes for different matches (ie a one-byte code for a short, close match, or a four-byte one for long-distance / long matches), but thats get complicated quickly and doesnt necessarily give us better compression (at least not in our intended usecases)
//  leave it as an option for later, then
//  the original used 15bit distance and 8b length
//  id say 7b length and 8b distance, and then we modify it as needed later on
// also, apparently i was correct in thinking that finding matches is gonna be the bitchiest part
// yeah i think we gon have to brute that :/
/* HAH! NVM I FOUND SOMETHING:
Duplicated strings are found using a hash table. All input strings of length 3 are inserted in the hash table. A hash index is computed for the next 3 bytes. If the hash chain for this index is not empty, all strings in the chain are compared with the current input string, and the longest match is selected.
The hash chains are searched starting with the most recent strings, to favor small distances and thus take advantage of the Huffman encoding. The hash chains are singly linked. There are no deletions from the hash chains, the algorithm simply discards matches that are too old.
To avoid a worst-case situation, very long hash chains are arbitrarily truncated at a certain length, determined by a runtime option (level parameter of deflateInit). So deflate() does not always find the longest possible match but generally finds a match which is long enough.
Deflate() also defers the selection of matches with a lazy evaluation mechanism. After a match of length N has been found, deflate() searches for a longer match at the next input byte. If a longer match is found, the previous match is truncated to a length of one (thus producing a single literal byte) and the process of lazy evaluation begins again. Otherwise, the original match is kept, and the next match search is attempted only N steps later.
The lazy match evaluation is also subject to a runtime parameter. If the current match is long enough, deflate() reduces the search for a longer match, thus speeding up the whole process. If compression ratio is more important than speed, deflate() attempts a complete second search even if the first match is already long enough.
The lazy match evaluation is not performed for the fastest compression modes (level parameter 1 to 3). For these fast modes, new strings are inserted in the hash table only when no match was found, or when the match is not too long. This degrades the compression ratio but saves time since there are both fewer insertions and fewer searches.
*/
// regarding pattern matching: (since our shortest matches gon be two bytes,) we can also just make a match*table[256*256], and store the linked lists of occurences there. that way, we can just index the appropriate list, instead of having to compare thru a list of all possible matches
//  ideally, use a hash table or something similar to amortise this and trade it off a bit. 65k of pointers is doable (especially since we dont need compression to be that fast), but its still a chore (and maybe not as portable)
//  if we stick to 2c, we could have a match*table[256], use the first byte as lookup, and compare thru the second one
// later on, the same text suggests using a huffman decoding table that fits the longest keys (ie 128 entries if our longest pattern is 7 bits),
// using that to look up what the first match is, and how many bits to shift the rest
//  or, if no match is found, it defers to another table/lookup tree where it can match more input
// also, have shorter matches occupy the lesser bitvalues, and if two symbols have the same bit length, have them in lexical order. that way, we can deduce the exact values from ONLY the length of the encodings
// also, inert matching (for deflate): after finding a match, it tries to find a longer one on the next char, and uses that if its longer
//  btw this is recursive, so it can keep repeating this as long as the matches keep getting longer

// XXX argh fuck it this is cpp syntax, and we wanted this to be ansi-c compliant...
//struct dp{int offset;dp*next;dp(int o,dp*n):offset(o),next(n){}}; // can use the offset to figure out what the exact pattern is
typedef struct dp{int offset;void*next;}dp;
dp*newdp(int o,void*n){
	dp*ret=(dp*)malloc(sizeof(dp));
	ret->offset=o;ret->next=n;
	return ret;
}
dp*table[127]; // allows us to hash via the first byte, and the actual entries are linked trees

void deflate(char*input,char*out){
/*
next string is xyz
if not in table
	output x, move to next byte
else
	compare all strings under entry xyz
	find longest
	output reference to that
	move that many forward
in both cases, insert reference to xyz in table afterwards
'if not in table' conveniently also takes care of starting conditions
*/

	char*alldata=input; // bc we advance input, but need to still be able to keep track of where we at
	// ... we could just keep an int telling us how far weve advanced...?
	// also 'alldata' sounds like the reddit handle of a neonazi techbro
	for(int i=0;i<127;i++)table[i]=0;
	while(*input){
		char a=*input;int maxlength=1;
		if(!table[a])*out++=a;
		else{
			int curlength,offset;
			for(dp*t=(dp*)table[a];t;t=(dp*)t->next){
				if(alldata[t->offset+1]==input[1]){
					curlength=0;
					while(input[curlength]==alldata[t->offset+curlength])
						curlength++;
					if(curlength>maxlength){
						maxlength=curlength;
						offset=t->offset;
					}
				}
			}
			if(maxlength<2)*out++=a; // no match found, or too short to be worth it
			else{
				// output pointer to that (length then offset)
				// this is the part thatll have to get weird once we have multiple codes
				*out++=maxlength|0x80;
				*out++=input-alldata-offset;
			}
		}

		// add reference to current in table
		table[a]=newdp(input-alldata,table[a]);
		input+=maxlength;
	}
	*out=0;
}

void inflate(char*input){
	// okay wow this ones easy
	char*data=(char*)malloc(1000),*dp=data;
	while(*input){
		int a=*input++,off;
		if(a<127)*dp++=a;
		else{
			a&=0x7f;off=*input++;
			while(a--)
				*dp++=*(dp-off);
		}
	}
	while(data!=dp)putchar(*data++);putchar(10);
}



// huffman
/*
encode:
make list of all symbols in string
go thru string, add up their occurences
figure out tree - gonna need a sorting algorithm
	discard all that dont occur
	also, just, idk, cache the tree or smth - transmitting that is not our problem rn
from tree, make a new list of symbols
	foreach symbol, store bit pattern and its length
go thru string and bitpack according to above

decode:
start at root node
foreach input bit
	follow branch
	is a symbol?
		output symbol
		reset to root

start working on decoder optimisation table? or do we kiss it for now?
i say we kiss it - might be a bit superfluous for now, but lets at least get it working so far

i think tromp had a pretty neat bitpacker
int getnext(){ // get next bit of input
  if(!current_index){
    current_index=read_mode;
    current_char=getchar();
  }else current_index--;
  return current_char>>current_index&1;
}
well not exactly a bitpacker (or very useful), but yeah

could use a lisp-like memory structure to handle the tree
will need two lists, one for the tree itself (first 127 values are the symbols, remainder are nodes), and the second are the frequencies (set to 0 if the current item is already included somewhere)
*/

void pcons(int at,int*cmem){
	if(cmem[at+1]){
		putchar('<');
		pcons(cmem[at],cmem);
		putchar(',');
		pcons(cmem[at+1],cmem);
		putchar('>');
	}else
		putchar(cmem[at]);
}

void enumerate(int at,int*cmem,char*codes,char*clen,int path,int plen){
	if(cmem[at+1]){
		path<<=1;plen++;
		enumerate(cmem[at  ],cmem,codes,clen,path  ,plen);
		enumerate(cmem[at+1],cmem,codes,clen,path+1,plen);
	}else{
		codes[cmem[at]-1]=path;clen[cmem[at]-1]=plen;
		//printf("%c gets code %8b %i\n",cmem[at],path,plen);
	}
}

char codes[127],clen[127]; // putting these separate bc figuring out how to calculate them is not yet relevant

void pack(char*input,char*out){
	int cons[1000],freq[1000],max=127,symb=0;
	for(int i=0;i<max;i++){
		cons[2*i  ]=i+1;
		cons[2*i+1]=0;
		freq[i]=0;
	} // i think

	// technically, this should iterate thru cons to match, but meh
	for(int i=0;input[i];i++){
		if(!freq[input[i]-1])symb++;
		freq[input[i]-1]++;
	}

	printf("frequencies:\n");for(int i=0;i<max;i++)
	if(freq[i])printf("%c:%i  ",cons[2*i],freq[i]);
	putchar(10);

	freq[0]=1000;symb--;
	// symb tells us how many unique symbols there are
	// which is useful, bc this is the number of iterations to build the tree
	for(int iter=0;iter<symb;iter++){
		// iterate thru items, find the two w/ the least frequency
		// add new item w/ the previous two as branches, append to cons[]
		//  set its frequency as the sum of the freqs of its children
		// set the frequencies of the children to 0
		int l=0,r=0;
		for(int i=0;i<max;i++)
			if(freq[i] && freq[i]<freq[l]){
				if(freq[i]<freq[r])l=r,r=i;
				else l=i;
			}

		// XXX make sure (l,r) are in the right order to construct a tree!

		//printf("picked (%i %i) (%i %i) at %i\n",l,r,freq[l],freq[r],max);

		cons[2*max]=l<<1;
		cons[2*max+1]=r<<1;
		freq[max]=freq[l]+freq[r];
		freq[l]=freq[r]=0;
		max++;
	}

	// uuuh idk output tree

	max--;printf("tree:\n");
	pcons(max<<1,cons);putchar(10);

	// generate actual codes
	// uuuh
	// maybe make this recursive: foreach branch, recursively iterate (while keeping track of sequence so far), and if symbol found, put in dict
	//char codes[127],clen[127];
	for(int i=0;i<127;i++)codes[i]=clen[i]=0;
	enumerate(max<<1,cons,codes,clen,0,0);
	printf("mappings (lengths denoted separately - i couldnt get printf to cooperate):\n");
	for(int i=0;i<127;i++)if(clen[i])printf("%c:%8b %i\n",cons[2*i],codes[i],clen[i]);

	char bytes,blen,at=0;int outb=0,outat=0;
	for(char*c=input;*c;c++){
		bytes=codes[*c-1],blen=clen[*c-1];
		printf("code %c : %8b:%i at %i\n",*c,bytes,blen,at);
		at+=blen;
		outb=(outb<<blen)|bytes;
		if(at>8){
			at-=8;
			out[outat++]=outb>>at;
			outb&=(1<<at)-1;
		}
	}
	printf("packed in %i bytes\n",outat);
}

void unpack(int*input){
	// working on it!
}





void main(){
	char*data="abcdecdeiifffffgggghhhabcdecdefimmcquineandimheretosayimmcquineandimnowdone",
	out[1000],i;

	printf("%s\n\nlzw:\n",data);
	lzw_encode(data,out);
	for(i=0;out[i];i++)printf("%2x ",out[i]);printf("\n%i bytes\n",i);
	lzw_decode(out);

	printf("\ndeflate:\n");
	deflate(data,out);
	for(i=0;out[i];i++)printf("%2x ",out[i]);printf("\n%i bytes\n",i);
	inflate(out);

	return;

	printf("\nhuffman:\n");
	pack(data,out);
	//for(int i=0;out[i];i++)printf("%2x ",out[i]);putchar(10);
	for(i=0;i<27;i++)printf("%2x ",(unsigned char)out[i]);printf("\n%i bytes\n",i);

}

// lzw seems to sometimes have issues when the string gets longer
//  ...mcquineandimnowdone$ seemed to trigger it
//  might still be a memory allocation issue

// okayyy, huffman encoding works! now, to figure out how to reverse it!

// https://www.cs.ucdavis.edu/~martel/122a/deflate.html <- thats the introductory one that doesnt say HOW to do stuff
// http://pnrsolution.org/Datacenter/Vol4/Issue1/58.pdf
// https://www.sobyte.net/post/2022-01/gzip-and-deflate/
// both found by looking up 'deflate finding matches'

