#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <map>
#include <iostream>
#include <fstream>
#define dataBlkSize 51
#define idxSize 52

namespace sjtu {
    template <class Key, class Value, class Compare = std::less<Key> >
    class BTree {
        friend class iterator;
    private:
        // Your private members go here
        typedef std::pair< Key, Value> value_type;
        struct idxNode{
            long pos;//自己的位置
            Key key[idxSize-1];
            int type;//type为零表示下一层为索引节点，为1表示下一层是数据节点
            long idx[idxSize];//指向它下一层的节点
            int len;//有效的儿子数
            idxNode():len(1),type(1),pos(-1){};
            idxNode(const idxNode &node){
                type=node.type;
                pos=node.pos;
                len=node.len;
                for(int i=0;i<len;i++) idx[i]=node.idx[i];
                for(int i=0;i<len-1;i++) key[i]=node.key[i];
            }
        };
        struct dataNode{
            int len;//有效记录数
            //不用存头尾，可以用nxt，lst为-1来代表
            value_type record[dataBlkSize];//存放有效记录的数组,同时保存了K值
            long pos;//自己的位置
            long nxt;//下一个数据块
            long lst;//上一个数据块
            dataNode():len(1),pos(-1),nxt(-1),lst(-1){};
            dataNode(const dataNode &node){
                pos=node.pos;
                nxt=node.nxt;
                lst=node.lst;
                len=node.len;
                for(int i=0;i<len;i++) {record[i].first=node.record[i].first;record[i].second=node.record[i].second;}
            }
        };//其实也可以像根一样，只存一个head的位置
        long ROOT;//保存根的位置,这里就不写入文件了，不然还要保存根的位置的位置.......就没完了
        //私有函数可以随意用指针操作，因为函数一定是在内存里运行的，只要用函数改变数据成员的值就行了
        std::fstream io;
        long HEAD;
        long insert(const Key &key,const Value &value,long POS) {
            //由于当前节点t已经在内存里，所以可以直接操作
            idxNode t=rdidx(POS);
            long newNode;
            int i;
            for(i=0;i<t.len-1;i++){//查找x所在的子树
                if(key<t.key[i]) break;//到达第二层
            }
            if(t.type==0) {
                newNode = insert (key, value, t.idx[i]);//递归过程，直到下一层为数据块
            }
            else {
                newNode = insertData(key,value, t.idx[i]);//添加数据
            }
            if(newNode==-1) return -1;
            else {              //有分裂，添加索引项,
                if(t.type==0) {return addIdxBlk(newNode,POS);}
                else return addDataBlk(newNode,POS);
            }
        }
        long insertData(const Key &key,const Value &value,long POS){//返回分裂后多出来的节点指针（不分裂返回NULL）
            dataNode t=rddata(POS);
            if(t.len<dataBlkSize){//x可以插入当前块中
                int i;
                for(i=t.len;i>0 && key<t.record[i-1].first;i--)
                {t.record[i].first=t.record[i-1].first;t.record[i].second=t.record[i-1].second;}
                t.record[i].first=key;
                t.record[i].second=value;
                (t.len)++;

                wrtdata(t);

                dataNode v=rddata(t.pos);

                return -1;
            }

            //分裂当前块
            dataNode newNode;
            int i,j;
            int max = dataBlkSize/2;
            newNode.len=max+1;
            for(i=max,j=dataBlkSize-1;i>=0&&t.record[j].first>key;--i)
            {newNode.record[i].first=t.record[j].first;newNode.record[i].second=t.record[j].second;j--;}  //多出来的一半数据块放在了右边,原来块的大小正好为dataBlkSize
            if(i>=0) {newNode.record[i].first=key;newNode.record[i].second=value;i--;}//插入在右边
            for(;i>=0;i--) {newNode.record[i].first=t.record[j].first;newNode.record[i].first=t.record[j].first;j--;}

            t.len=dataBlkSize - max;   //修改左块的大小
            if(j<t.len-1){         //x没有被插入到新数据块中
                for(;j>=0&&key<t.record[j].first;--j) {t.record[j+1].first=t.record[j].first;t.record[j+1].second=t.record[j].second;}
                t.record[j+1].first=key;t.record[j+1].second=value;
            }

            //把修改后的两个节点写回文件
            if(t.nxt!=-1){//如果后面还有节点
                dataNode tmp=rddata(t.nxt);

                newNode.nxt=t.nxt;//多出来的数据块的下一个是原数据块的下一个
                newNode.lst=t.pos;
                io.clear();
                io.seekp(0,std::ios::end);
                tmp.lst=t.nxt=newNode.pos=io.tellp();

                idxNode root=rdidx(ROOT);

                wrtdata(newNode);
                root=rdidx(ROOT);

                wrtdata(t);
                long o=t.pos;
                long y=sizeof(dataNode);

                root=rdidx(ROOT);

                long x=io.tellp();

                wrtdata(tmp);
                root=rdidx(ROOT);

            }
            else{//如果后面没有节点
                newNode.nxt=-1;
                newNode.lst=t.pos; //要保存上一个结点的位置
                io.clear();
                io.seekp(0,std::ios::end);
                t.nxt=newNode.pos=io.tellp();


                idxNode root=rdidx(ROOT);

                wrtdata(newNode);
                wrtdata(t);

            }
            idxNode root=rdidx(ROOT);
            return newNode.pos;
        }

        long addIdxBlk(long POS1,long POS2){
            idxNode p=rdidx(POS1);//新分裂出来的idxNode
            idxNode newNode=rdidx(POS1);
            idxNode t=rdidx(POS2);//上层

            //找新插入块的最小值存入min
            while(p.type==0) p=rdidx(p.idx[0]);
            dataNode d=rddata(p.idx[0]);
            Key min=d.record[0].first;

            if(t.len<idxSize){         //索引块没有满，直接加入
                int i;
                for(i=t.len-1;i>0&&min<t.key[i-1];--i){
                    t.key[i]=t.key[i-1];
                    t.idx[i+1]=t.idx[i];
                }
                t.key[i]=min;
                t.idx[i+1] =newNode.pos;
                ++(t.len);
                wrtidx(t);

                return -1;
            }
            //分裂当前结点
            idxNode newIdx;
            newIdx.type=0;
            int max=idxSize/2;
            newIdx.len=max+1;
            int i,j;

            if(min>t.key[idxSize-2]){      //新插入的项是最大的，移到新索引块
                newIdx.key[max-1]=min;
                newIdx.idx[max]=newNode.pos;
            }
            else{
                newIdx.key[max-1]=t.key[idxSize-2];
                newIdx.idx[max]=t.idx[idxSize-1];
                for(i=t.len-2;i>0&&min<t.key[i-1];--i){
                    t.key[i]=t.key[i-1];
                    t.idx[i+1]=t.idx[i];
                }
                t.key[i]=min;
                t.idx[i+1]=newNode.pos;//先不分裂，而是把最后一个弹出去，再把新的索引加进来
            }
            //分裂一半索引项到新增索引结点
            for(i=max-1,j=idxSize-1;i>0;--i,--j){
                newIdx.idx[i]=t.idx[j];
                newIdx.key[i-1]=t.key[j-1];
            }
            newIdx.idx[0]=t.idx[j];//这两个指的是同一个块
            t.len=idxSize-max;
            wrtidx(t);
            io.seekp(0,std::ios::end);
            newIdx.pos=io.tellp();
            wrtidx(newIdx);

            return newIdx.pos;
        }
        long addDataBlk(long POS1,long POS2){//数据块有分裂，处理上一层索引块
            idxNode t=rdidx(POS2);
            dataNode newNode=rddata(POS1);

            if(t.len<idxSize){//当前块还没有满，直接插入
                int i;
                for(i=t.len-1;i>0&&newNode.record[0].first<t.key[i-1];--i){
                    t.key[i]=t.key[i-1];
                    t.idx[i+1] =t.idx[i];
                }
                t.key[i] =newNode.record[0].first;
                t.idx[i+1] =newNode.pos;
                ++(t.len);
                wrtidx(t);//把修改后的索引块写回去
                return -1;
            }
            //分裂结点(索引块)
            idxNode newIdx;
            newIdx.type=1;
            int max=idxSize/2;
            newIdx.len = max+1;
            int i,j;

            if(newNode.record[0].first >t.key[idxSize-2]){//新增加的数据块是最大的，把分裂出来的索引块的最后一个索引写成新分裂出来的数据块中的最小值
                newIdx.key[max-1] = newNode.record[0].first;
                newIdx.idx[max]=newNode.pos;
            }
            else {
                newIdx.key[max-1]=t.key[idxSize-2];
                newIdx.idx[max]=t.idx[idxSize-1];//把原索引块的最后一项弹走，len==idxsize

                for(i=t.len-2;i>0&&newNode.record[0].first<t.key[i-1];--i){
                    t.key[i]=t.key[i-1];
                    t.idx[i+1]=t.idx[i];
                }
                t.key[i]=newNode.record[0].first;
                t.idx[i+1]=newNode.pos;//把新的索引写入（此时还没有进行分裂，只是写入之前先把最后一个索引弹走了
            }

            //将一半索引项移到新索引块
            for(i=max-1,j=idxSize-1;i>0;--i,--j){
                newIdx.idx[i]=t.idx[j];
                newIdx.key[i-1]=t.key[j-1];
            }
            newIdx.idx[0]=t.idx[j];//劈开之后需要多出来一个idx，设为与上一个idx相同
            t.len=idxSize - max;
            wrtidx(t);
            io.seekp(0,std::ios::end);
            newIdx.pos=io.tellp();
            wrtidx(newIdx);

            return newIdx.pos;
        }
        void idxNode_copy(long POS,std::fstream IO){//这里的*t指向other中的将要被抄进来的结点
            idxNode node;
            IO.seekg(POS);
            IO.read(reinterpret_cast<char *> (node),sizeof(idxNode));
            wrtdata(node);//一开始会在相同位置再写一次根
            if(node.type==0){//如果下面还有需要抄的节点，没有的话直接返回即可（因为此节点已经被抄好了
                for(int i=0;i<node.len;i++) {
                    idxNode newNode=rdidx(node.idx[i]);
                    idxNode_copy(newNode.pos,IO);
                }
            }
            return ;
        }
        void wrtidx(idxNode &node) {
            io.clear();
            io.seekp(0,std::ios::beg);
            long t=io.tellp();
            io.seekp(node.pos,std::ios::beg);
            io.write(reinterpret_cast<char *> (&node),sizeof(idxNode));

            io.flush();
        }
        void wrtdata(dataNode &node) {
            io.clear();
            io.seekp(0,std::ios::beg);
            long t=io.tellp();
            io.seekp(node.pos,std::ios::beg);
            io.write(reinterpret_cast<char *> (&node.len),sizeof(int));
            io.write(reinterpret_cast<char *> (&node.record[0]),sizeof(node.record));
            io.write(reinterpret_cast<char *> (&node.pos),sizeof(int));
            io.write(reinterpret_cast<char *> (&node.nxt),sizeof(int));
            io.write(reinterpret_cast<char *> (&node.lst),sizeof(int));

            io.flush();

        }
        idxNode rdidx(long offset){
            io.clear();
            io.seekg(0,std::ios::beg);
            long t=io.tellg();
            io.seekg(offset,std::ios::beg);
            idxNode node;
            io.read(reinterpret_cast<char *> (&node),sizeof(idxNode));

            io.flush();

            return node;
        }
        dataNode rddata(long offset){
            io.clear();
            io.seekg(0,std::ios::beg);
            long t=io.tellg();
            io.seekg(offset,std::ios::beg);
            dataNode node;
            io.read(reinterpret_cast<char *> (&node),sizeof(dataNode));

            io.flush();

            return node;
        }

    public:
        //typedef std::pair<const Key, Value> value_type;
        class const_iterator;
        class iterator {
            friend class Btree;
        private:
            // Your private members go here
            long pos;//保存位置
            int idx;//保存下标
            int len;//保存这一块中有效的数据数
            std::fstream io;
        public:
            bool modify(const Value& value){//这个函数是干什么用的？？？
                return 1;
            }
            iterator() {
                //  Default Constructor
                //io.open("file");
                if(!io) throw 1;
            }
            iterator(const iterator& other) {
                //  Copy Constructor
                pos=other.pos;
                idx=other.idx;
                len=other.len;
                //io.open("file");
                if(!io) throw 1;
            }
            // Return a new iterator which points to the n-next elements
            iterator operator++(int) {
                //  iterator++
                iterator tmp=*this;
                if(idx<len-1) idx+=1;//如果还在这一块里
                else{//如果要跳到下一块
                    dataNode cur;
                    io.seekg(pos);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));//读出数据块
                    pos=cur.nxt;
                    idx=0;
                    io.seekg(cur.nxt);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));
                    len=cur.len;//读出下一个数据块的len
                }
                return tmp;
            }
            iterator& operator++() {
                //  ++iterator
                if(idx<len-1) idx+=1;//如果还在这一块里
                else{//如果要跳到下一块
                    dataNode cur;
                    io.seekg(pos);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));//读出数据块
                    pos=cur.nxt;
                    idx=0;
                    io.seekg(cur.nxt);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));
                    len=cur.len;//读出下一个数据块的len
                }
                return *this;
            }
            iterator operator--(int) {
                //  iterator--
                iterator tmp=*this;
                if(idx>0) idx-=1;//如果还在这一块里
                else{//如果要跳到上一块
                    dataNode cur;
                    io.seekg(pos);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));//读出数据块
                    pos=cur.lst;
                    io.seekg(cur.lst);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));
                    len=cur.len;//读出上一个数据块的len
                    idx=len-1;
                }
                return tmp;
            }
            iterator& operator--() {
                //  --iterator
                if(idx>0) idx-=1;//如果还在这一块里
                else{//如果要跳到上一块
                    dataNode cur;
                    io.seekg(pos);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));//读出数据块
                    pos=cur.lst;
                    io.seekg(cur.lst);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));
                    len=cur.len;//读出上一个数据块的len
                    idx=len-1;
                }
                return *this;
            }
            // Overloaded of operator '==' and '!='
            // Check whether the iterators are same
            bool operator==(const iterator& rhs) const {
                // Todo operator ==
                if(pos==rhs.pos&&idx==rhs.pos) return 1;
                else return 0;
            }
            bool operator==(const const_iterator& rhs) const {
                // Todo operator ==
                if(pos==rhs.pos&&idx==rhs.pos) return 1;
                else return 0;
            }
            bool operator!=(const iterator& rhs) const {
                // Todo operator !=
                if(pos==rhs.pos&&idx==rhs.pos) return 0;
                else return 1;
            }
            bool operator!=(const const_iterator& rhs) const {
                // Todo operator !=
                if(pos==rhs.pos&&idx==rhs.pos) return 0;
                else return 1;
            }
        };
        class const_iterator {
            // it should has similar member method as iterator.
            //  and it should be able to construct from an iterator.
        private:
            // Your private members go here
            long pos;
            int idx;//保存下标
            int len;//保存这一块中有效的数据数
        public:
            const_iterator() {
                //
                io.open("file");
                if(!io) throw 1;
            }
            const_iterator(const const_iterator& other) {
                //
                pos=other.pos;
                idx=other.idx;
                len=other.len;
            }
            const_iterator(const iterator& other) {

                pos=other.pos;
                idx=other.idx;
                len=other.len;
            }
            // And other methods in iterator, please fill by yourself.
            const_iterator operator++(int) {
                //  iterator++
                iterator tmp=*this;
                if(idx<len-1) idx+=1;//如果还在这一块里
                else{//如果要跳到下一块
                    dataNode cur;
                    io.seekg(pos);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));//读出数据块
                    pos=cur.nxt;
                    idx=0;
                    io.seekg(cur.nxt);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));
                    len=cur.len;//读出下一个数据块的len
                }
                return tmp;
            }
            const_iterator& operator++() {
                //  ++iterator
                if(idx<len-1) idx+=1;//如果还在这一块里
                else{//如果要跳到下一块
                    dataNode cur;
                    io.seekg(pos);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));//读出数据块
                    pos=cur.nxt;
                    idx=0;
                    io.seekg(cur.nxt);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));
                    len=cur.len;//读出下一个数据块的len
                }
                return *this;
            }
            const_iterator operator--(int) {
                //  iterator--
                iterator tmp=*this;
                if(idx>0) idx-=1;//如果还在这一块里
                else{//如果要跳到上一块
                    dataNode cur;
                    io.seekg(pos);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));//读出数据块
                    pos=cur.lst;
                    io.seekg(cur.lst);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));
                    len=cur.len;//读出上一个数据块的len
                    idx=len-1;
                }
                return tmp;
            }
            const_iterator& operator--() {
                //  --iterator
                if(idx>0) idx-=1;//如果还在这一块里
                else{//如果要跳到上一块
                    dataNode cur;
                    io.seekg(pos);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));//读出数据块
                    pos=cur.lst;
                    io.seekg(cur.lst);
                    io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));
                    len=cur.len;//读出上一个数据块的len
                    idx=len-1;
                }
                return *this;
            }
            // Overloaded of operator '==' and '!='
            // Check whether the iterators are same
            bool operator==(const iterator& rhs) const {
                //  operator ==
                if(pos==rhs.pos&&idx==rhs.pos) return 1;
                else return 0;
            }
            bool operator==(const const_iterator& rhs) const {
                //  operator ==
                if(pos==rhs.pos&&idx==rhs.pos) return 1;
                else return 0;
            }
            bool operator!=(const iterator& rhs) const {
                //  operator !=
                if(pos==rhs.pos&&idx==rhs.pos) return 0;
                else return 1;
            }
            bool operator!=(const const_iterator& rhs) const {
                //  operator !=
                if(pos==rhs.pos&&idx==rhs.pos) return 0;
                else return 1;
            }

        };
        // Default Constructor and Copy Constructor
        BTree() {
            //  Default
            std::ifstream infile("file");
            if(!infile) {
                std::ofstream outfile("file");
                outfile.close();
                io.open("file", std::ios::in | std::ios::out | std::ios::binary);
                if (!io) { std::cerr << "nomatch"; }
                ROOT = -1;
                HEAD = -1;

                io.seekp(0,std::ios::beg);
                io.write(reinterpret_cast<char *> (&ROOT),sizeof(int));
                io.write(reinterpret_cast<char *> (&HEAD),sizeof(int));
                io.flush();

            }
            else {
                std::ofstream outfile("file");
                outfile.close();
                io.open("file", std::ios::in | std::ios::out | std::ios::binary);
                if (!io) { std::cerr << "nomatch"; }
                io.seekg(0,std::ios::beg);
                io.write(reinterpret_cast<char *> (&ROOT),sizeof(int));
                io.write(reinterpret_cast<char *> (&HEAD),sizeof(int));

            }
        }
        BTree(const BTree& other) {
            //  Copy
            std::ofstream outfile("file");
            io.open("file");
            if(!io) throw 1;
            ROOT=other.ROOT;
            HEAD=other.HEAD;
            //写根节点
            idxNode root;
            std::fstream IO=other.io;
            IO.seekg(other.ROOT);
            IO.read(reinterpret_cast<char *> (&root),sizeof(idxNode));
            io.seekp(ROOT);
            io.write(reinterpret_cast<char *> (&root),sizeof(dataNode));

            //写头结点
            dataNode head;
            IO.seekg(other.HEAD);
            IO.read(reinterpret_cast<char *> (&head),sizeof(idxNode));
            io.seekp(HEAD);
            io.write(reinterpret_cast<char *> (&head),sizeof(dataNode));

            //写索引节点
            idxNode_copy(ROOT,IO);

            //写数据块
            dataNode t;
            t.pos=head.nxt;
            while(t.pos!=-1){
                std::fstream IO=other.io;
                IO.seekg(t.pos);
                IO.read(reinterpret_cast<char *> (&t),sizeof(dataNode));
                io.seekp(t.pos);
                io.write(reinterpret_cast<char *> (&t),sizeof(dataNode));
                t.pos=t.nxt;
            }

        }
        BTree& operator=(const BTree& other) {//和拷贝构造函数完全相同
            //  Assignment
            std::ofstream outfile("file");
            io.open("file");
            if(!io) throw 1;
            ROOT=other.ROOT;
            HEAD=other.HEAD;

            //写根节点
            idxNode root;
            std::fstream IO=other.io;
            IO.seekg(other.ROOT);
            IO.read(reinterpret_cast<char *> (&root),sizeof(idxNode));
            io.seekp(ROOT);
            io.write(reinterpret_cast<char *> (&root),sizeof(dataNode));

            //写头结点
            dataNode head;
            IO.seekg(other.HEAD);
            IO.read(reinterpret_cast<char *> (&head),sizeof(idxNode));
            io.seekp(HEAD);
            io.write(reinterpret_cast<char *> (&head),sizeof(dataNode));

            //写索引节点
            idxNode_copy(&root,other);

            //写数据块
            dataNode t;
            t.pos=head.nxt;
            while(t.pos!=-1){
                std::fstream IO=other.io;
                IO.seekg(t.pos);
                IO.read(reinterpret_cast<char *> (&t),sizeof(dataNode));
                io.seekp(t.pos);
                io.write(reinterpret_cast<char *> (&t),sizeof(dataNode));
                t.pos=t.nxt;
            }
            return *this;
        }
        ~BTree() {
            //  Destructor
            io.close();
        }
        // Insert: Insert certain Key-Value into the database
        // Return a pair, the first of the pair is the iterator point to the new
        // element, the second of the pair is Success if it is successfully inserted
        void insert(const Key& key, const Value& value) {
            //  insert function

            if(ROOT==-1){//空树的插入
                idxNode root;
                root.type=1;
                root.len=1;

                dataNode head;
                io.clear();
                head.len=0;
                io.seekp(0,std::ios::end);
                HEAD=head.pos=io.tellp();
                wrtdata(head);

                //添加数据块
                dataNode p;
                p.record[0].first=key;
                p.record[0].second=value;
                p.lst=HEAD;
                io.open("file");
                io.clear();
                io.seekp(0,std::ios::end);
                p.pos=io.tellp();
                wrtdata(p);

                //修改头结点状态
                io.open("file");
                head.nxt=p.pos;
                wrtdata(head);

                //生成根
                root.idx[0]=p.pos;
                io.open("file");
                io.clear();
                io.seekp(0,std::ios::end);
                ROOT=root.pos=io.tellp();
                wrtidx(root);

            }

            idxNode tmp=rdidx(ROOT);
            long p = insert(key,value,ROOT);//传入的是root的地址，相当于指向root的指针
            if(p!=-1){    //原根节点被分裂了，处理树增高

                idxNode newNode=rdidx(p);

                idxNode t;//t代表新的根
                t.type=0;
                t.len=2;
                t.idx[0]=ROOT;
                t.idx[1]=p;

                //寻找第二块的最小值
                while(newNode.type==0)  newNode=rdidx(newNode.idx[0]);

                dataNode d=rddata(newNode.idx[0]);//读出最小值所在的数据块

                t.key[0]=d.record[0].first;
                io.open("file");
                io.clear();
                io.seekp(0,std::ios::end);
                ROOT=t.pos=io.tellp();
                wrtidx(t);

            }

        }
        // Erase: Erase the Key-Value
        // Return Success if it is successfully erased
        // Return Fail if the key doesn't exist in the database
        void erase(const Key& key) {
            // TODO erase function
             // If you can't finish erase part, just remaining here.
        }
        // Return a iterator to the beginning
        iterator begin() {
            iterator it;
            it.pos=HEAD;//直接指向头节点
            return it;
        }
        const_iterator cbegin()  {
            iterator it;
            it.pos=HEAD;//直接指向头节点
            return it;
        }
        // Return a iterator to the end(the next element after the last)
        iterator end() {
            iterator it;
            dataNode cur;
            io.seekg(HEAD);
            io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));//读出数据块
            while(cur.nxt!=-1){
                io.seekg(cur.nxt);
                io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));
            }
            it.pos=cur.pos;
            it.len=cur.len;
            it.idx=it.len-1;
            return it;
        }
        const_iterator cend()  {
            iterator it;
            dataNode cur;
            io.seekg(HEAD);
            int w;
            io.read((char *)&w,sizeof(int));
            io.read(reinterpret_cast<char *> (&w),sizeof(int));
            io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));//读出数据块
            while(cur.nxt!=-1){
                io.seekg(cur.nxt);
                io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));
            }
            it.pos=cur.pos;
            it.len=cur.len;
            it.idx=it.len-1;
            return it;
        }
        // Check whether this BTree is empty
        bool empty()  {
            if(ROOT==-1) return 1;
            else return 0;
        }
        // Return the number of <K,V> pairs
        size_t size()  {
            dataNode cur;
            int cnt=0;
            io.seekg(HEAD);
            io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));//读出数据块
            while(cur.nxt!=-1){
                io.seekg(cur.nxt);
                io.read(reinterpret_cast<char *> (&cur),sizeof(dataNode));
                cnt+=cur.len;
            }
            return cnt;
        }
        // Clear the BTree
        void clear() {
            ROOT=-1;
        }
        // Return the value refer to the Key(key)
        Value at(const Key& key){

            idxNode cur=rdidx(ROOT);

            //在索引块向下找
            int i;
            while(true) {
                for (i = 0; i < cur.len - 1; i++) {//查找所在的子树
                    if (key < cur.key[i]) break;
                }
                if(cur.type==1) break;
                cur=rdidx(cur.idx[i]);
            }
            //在数据块中找
            dataNode tmp=rddata(cur.idx[i]);//打开数据块
            for(i=0;i<tmp.len;i++){
                if(tmp.record[i].first==key) break;
                if(i==tmp.len) throw "no match";
            }
            return tmp.record[i].second;
        }
        /**
         * Returns the number of elements with key
         *   that compares equivalent to the specified argument,
         * The default method of check the equivalence is !(a < b || b > a)
         */
        size_t count(const Key& key)  {
            //这个函数是不是只能返回0或者1
            idxNode cur;
            io.seekg(ROOT);
            io.read(reinterpret_cast<char *> (&cur),sizeof(idxNode));

            //在索引块向下找
            int i;
            while(true) {
                for (i = 0; i < cur.len - 1; i++) {//查找所在的子树
                    if (key < cur.key[i]) break;
                }
                if(cur.type==1) break;
                io.seekg(cur.idx[i]);
                io.read(reinterpret_cast<char *> (&cur),sizeof(idxNode));
            }

            //在数据块中找
            io.seekg(cur.idx[i]);
            dataNode tmp;
            io.read(reinterpret_cast<char *> (&tmp),sizeof(dataNode));//打开数据块
            for(i=0;i<tmp.len;i++){
                if(!(tmp.record[i].first<key||tmp.record[i].first>key)) return 1;
            }
            return 0;
        }
        /**
         * Finds an element with key equivalent to key.
         * key value of the element to search for.
         * Iterator to an element with key equivalent to key.
         *   If no such element is found, past-the-end (see end()) iterator is
         * returned.
         */
        iterator find(const Key& key) {
            idxNode cur;
            io.seekg(ROOT);
            io.read(reinterpret_cast<char *> (&cur),sizeof(idxNode));

            //在索引块向下找
            int i;
            while(true) {
                for (i = 0; i < cur.len - 1; i++) {//查找所在的子树
                    if (key < cur.key[i]) break;
                }
                if(cur.type==1) break;
                io.seekg(cur.idx[i]);
                io.read(reinterpret_cast<char *> (&cur),sizeof(idxNode));
            }

            //在数据块中找
            io.seekg(cur.idx[i]);
            dataNode tmp;
            io.read(reinterpret_cast<char *> (&tmp),sizeof(dataNode));//打开数据块
            for(i=0;i<tmp.len;i++){
                if(tmp.record[i].first==key) break;
            }
            //没找到
            if(i==tmp.len) return end();

            iterator  it;
            it.pos=tmp.pos;
            it.len=tmp.len;
            it.idx=i;
            return it;
        }
        /*const_iterator find(const Key& key) {
            idxNode cur;
            io.seekg(ROOT);
            io.read(reinterpret_cast<char *> (&cur),sizeof(idxNode));
            //在索引块向下找
            int i;
            while(true) {
                for (i = 0; i < cur.len - 1; i++) {//查找所在的子树
                    if (key < cur.key[i]) break;
                }
                if(cur.type==1) break;
                io.seekg(cur.idx[i]);
                io.read(reinterpret_cast<char *> (&cur),sizeof(idxNode));
            }
            //在数据块中找
            io.seekg(cur.idx[i]);
            dataNode tmp;
            io.read(reinterpret_cast<char *> (&tmp),sizeof(dataNode));//打开数据块
            for(i=0;i<tmp.len;i++){
                if(tmp.record[i].first==key) break;
            }
            //没找到
            if(i==tmp.len) return end();
            const_iterator  it;
            it.pos=tmp.pos;
            it.len=tmp.len;
            it.idx=i;
            return it;
        }*/
    };
}   //namespace sjtu
