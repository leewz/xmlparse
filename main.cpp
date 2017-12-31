//third attempt
//just read EVERYTHING into a buffer and then deal with it later.

//ugh, there are attrvals with leading whitespace. whyyy

#include<map>
#include<set>
#include<vector>
#include<functional>
#include<iostream>
#include<fstream>
#include<cassert>
#include<cstdlib>


#define private public

using namespace std;

typedef char const* cstr;

//pointer string
struct pstr{
	cstr b,e; //beginning, end of string
	
	operator string() const{
		return string(b,e);
	}
	
	//why am i writing iterators for this
	cstr const begin() const{
		return b;
	}
	cstr const end() const{
		return e;
	}
	
};

class node{
private:
	static size_t ncount;
	static size_t const PSIZE;
	static vector<std::vector<char>> pool_list;
public:
	
	static void * operator new(size_t sz);
	static void operator delete(void*p);
	
	//don't allow arrays
	static void * operator new[](size_t sz)=delete;
	static void operator delete[](void*p)=delete;
	
	virtual ~node()=0;
};
node::~node(){}

//manual memory management saves about a second
/*		536367  tag
		358189  internal
		318020  content
536367*sizeof(tnode)+358189*(sizeof(inode - sizeof(tnode)))+318020*sizeof(pnode)
*/

size_t node::ncount = 0; //number of nodes created

size_t const node::PSIZE = 0x20000000;

std::vector<std::vector<char>> node::pool_list(1,vector<char>(PSIZE));
void * node::operator new(size_t sz){
	static auto free = pool_list.back().begin(); //index of free pool
	static auto end = pool_list.back().end();
	ncount++;
	free+=sz;

	//expand pool if necessary
	assert(free<=end);
//	if(free > end){
//		std::cerr<<"Expanding mempool\n";
//		pool_list.emplace_back(PSIZE); //add another pool
//		free = pool_list.back().begin();
//		end = pool_list.back().end();
//		free+=sz;
//	}
	
	return (void*)&*(free-sz);
}

void node::operator delete(void*p){
	static size_t ndel = 0;//number of deleted nodes
	
	if(++ndel==ncount){
		//delete everything
		pool_list.clear();
	}
}

//static char const SPACES[]=" \r\n\t";
static char const SPACES = ' ';

//tag node
class tnode: public node{
private:
	pstr tag;//the name of the tag
	static const int MAX_ATTR = 15;
	pstr keys[MAX_ATTR], vals[MAX_ATTR];
	int attrc;//attribute count
public:
	tnode(pstr const);
};

//assume only one space
//assumes no trailing space
tnode::tnode(pstr const str)
:attrc(0)
{
	register cstr p, q = str.b, r, s, e=str.e;
	while(++q<e&&*q!=' ');
	
	//right now q is at a space
	tag = {str.b,q};
	
	while(++q<e){
		//advance
		p=q;

		while(++q<e&&*q != '=');
		
		if(q>=e) //did not find a next attr
			return;
		
		if(attrc>=MAX_ATTR)
			throw "out of bounds";
		
		r=q+1;

		s=r;
		switch(*r){
		case '\'':
		case '"':
			while(*++s!=*r);
			keys[attrc] = {p, q};
			vals[attrc] = {r+1, s};
			attrc++;
			q=s+1;
			break;
		default:
			cerr<<"they DO exist!\n";
			while(++s<e && *s!=' ');
			keys[attrc] = {p, q};
			vals[attrc] = {r, s};
			attrc++;
			q=s;
			break;
		}
	}
}

//internal node
//like <p>content</p>
//can have children
struct inode:public tnode{
//private:
	vector<node*>children;

public:
	inode(pstr const);
	
	virtual ~inode();

	void add(node* n);
	
	//delete these operators for now
	inode(tnode const&) = delete;
	inode& operator=(tnode const&) = delete;
	//move should be fine...
	inode& operator=(tnode &&) = delete;
	
	friend class parser;
};

inline inode::inode(pstr const str):tnode(str){}

inode::~inode(){ //needs to be virtual
	for(auto child:children)
		delete child;
}

inline void inode::add(node* n){
	children.push_back(n);
}

class pnode:public node{//plaintext node, also known as "not tagged"
private:
	pstr content; //start and end of string content
public:
	pnode(pstr const str){content = str;}
		//not translated yet from xml
		//should also collapse whitespace
};


class document{
private:
	vector<char>buffer;
	inode head; //dummy head node
	inode* stack[20];//stack of node depth
	inode** stackptr; //pointer into the stack
	inode* root;
	
public:
	document(document const&) = delete;
	document& operator=(document const&) = delete;
	
	document(string const& fname)
	:buffer(0x08000000)//128MB should be enough for anything
	,head({&buffer[0],&buffer[0]})//at least it's a valid memory location
	,stackptr(stack)
	{
		ifstream file(fname);
		
		if(!file){
			throw "Failure to open "+fname;
		}
		
		file.read(&buffer[0], buffer.size());
		
		//if the buffer filled up, quit
		if(file.gcount()>=buffer.size()){
			cerr<<"buffer full\n";
			exit(1);
		}
		
		buffer.resize(file.gcount()); //to keep size
		
		*stackptr++ = &head; //put the dummy on the stack
		
		cstr left=&buffer[0]-1;
		cstr right=left;
		cstr const bufend = left+buffer.size(); //slightly smaller
		
		while(true){
			//find <
			while(*++left != '<' && left < bufend);
			if(left>=bufend) break;
			
			//add content
			contentwise(right+1, left);
			
			//find >
			right = left;
			while(*++right != '>');// && right<bufend);
			
			//add tag
			tagwise(left+1, right);
		}
		//find the root
		for(auto p : head.children){
			root = dynamic_cast<inode*>(p);
			if(root)
				break;
		}
		
		file.close(); //just in case
	}
	
	void tagwise(cstr b, cstr e){
		if(*b=='/')//end tag
			--stackptr;
		else if(e[-1]=='/'){ //empty tag
			stackptr[-1]->add(new tnode({b,e-1})); //add without the /
		}else{//it's the start of an internal node
			inode* temp = new inode({b,e});
			stackptr[-1]->add(temp);
			*stackptr++ = temp;
		}
	}
	
	void contentwise(cstr b, cstr e){
		if(b>=e)return;
		char * i = const_cast<char*>(b);
		
		//true if the last character was a whitespace
		bool whitespaced = false;
		//assume empty first
		bool empty = true; //don't print space if empty
		//cleanup
		for(auto p=b;p<e;p++){
			switch(*p){
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				whitespaced=true;break;
			default:
				if(whitespaced && !empty){
					*i++ = ' ';
					whitespaced=false;
				}
				*i++ = *p;
				empty = false;
			}
		}
		*i=0;
		if(b<i)
			stackptr[-1]->add(new pnode({b,i}));
	}
};

void traverse(inode* cur, function<void(inode*, node*)> f){
	for(node*p:cur->children){
		f(cur,p);
		auto n = dynamic_cast<inode*>(p);
		if(n)//if it's an internal node
			traverse(n,f);
	}
}

void nodetypes(inode*root){
	int internal=0, tag=0, content=0;
	traverse(root,
			[&](inode*parent, node*p){
				if(dynamic_cast<tnode*>(p))tag++;
				if(dynamic_cast<inode*>(p))internal++;
				if(dynamic_cast<pnode*>(p))content++;
			});
	cout
		<<tag<<"\ttag\n"
		<<internal<<"\tinternal\n"
		<<content<<"\tcontent\n"
	;
}
void instances(inode*root){
	map<string, int> m;
	
	traverse(root,
			[&](inode*parent, node*p){
				inode* p2 = dynamic_cast<inode*>(p);
				if(p2){
					m[p2->tag]++;
				}
			});
	int sum=0;
	for(auto kv:m){
		cout<<kv.second<<" "<<kv.first<<endl;
		sum+=kv.second;
	}
	cout<<sum<<endl;
}

//want to see the attributes
void attrtypes(inode* root){
	//tag, attribute, values
	map<string, map<string, map<string,int>>> m;
	traverse(root,
			[&](inode *parent, node *n){
				tnode* n2 = dynamic_cast<tnode*>(n);
				if(n2){
					auto& blargh =  m[n2->tag]; //list of key/val pairs for that attribute
					for(int i = 0; i < n2->attrc; i++){
						blargh[n2->keys[i]][n2->vals[i]]++;
					}
				}
			});
			
	for(auto const & tag:m){
		cout<<tag.first<<endl;
		for(auto const& key: tag.second){
			cout<<'\t'<<key.first<<endl;
			for(auto const& val : key.second){
				cout<<"\t\t"<<val.first<<"\t\t\t"<<val.second<<endl;
			}
		}
	}
}
//what kind of children can each type of child have?
void followers(inode* root){
	map<string,map<string,int>> blargh;
	map<inode*, int> ptrset;
	traverse(root, [&](inode* parent, node*p){
			if(ptrset[parent]++)
				return;
			auto & bleh = blargh[parent->tag];
			
			for(auto child:parent->children){
				tnode *const p = dynamic_cast<tnode * const>(child);
				if(p)
					bleh[p->tag]++;
				else bleh["<text>"]++;
			}
	});

	//print map
	for(auto const& pair:blargh){
		cout<<pair.first<<endl;
		for(auto child : pair.second)
			cout<<'\t'<<child.first<<"\t\t\t"<<child.second<<endl;
	}
}

int main(){
	string fname = string(std::getenv("APPDATA"))+"/CBLoader/combined.dnd40";
	
	cerr<<"Loading (this may take up to a minute)..."<<endl;
	document doc(fname);
	cerr<<"Loaded"<<endl;
	
//	nodetypes(doc.root);
//	instances(doc.root);
//	attrtypes(doc.root);
//	followers(doc.root);
	
	return 0;
}
