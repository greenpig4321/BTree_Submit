//
// Created by 郑文鑫 on 2019-03-09.
//
#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <map>
#include <iostream>
#include <fstream>
#define dataBlkSize 50
#define idxSize 200
namespace sjtu {
    template <class Key, class Value, class Compare = std::less<Key> >
    class BTree {
    friend class iterator;
    private:
        // Your private members go here
        typedef std::pair<const Key, Value> value_type;
        struct idxNode{
            int type;//type为零表示下一层为索引节点，为1表示下一层是数据节点
            Key key[idxSize-1];
            long pos;//自己的位置
            long idx[idxSize];//指向它下一层的节点
            int len;//有效的儿子数
            idxNode():len(1){};
        };
        struct dataNode{
            int len;//有效记录数
            //不用存头尾，可以用nxt，lst为-1来代表
            value_type record[dataBlkSize];//存放有效记录的数组,同时保存了K值
            long pos;//自己的位置
            long nxt;//下一个数据块
            long lst;//上一个数据块
            dataNode():len(1),lst(-1),nxt(-1){};
        }head;//其实也可以像根一样，只存一个head的位置
        long ROOT;//保存根的位置,这里就不写入文件了，不然还要保存根的位置的位置.......就没完了
        //私有函数可以随意用指针操作，因为函数一定是在内存里运行的，只要用函数改变数据成员的值就行了
        std::fstream io;
        idxNode *insert(const Key &key,const Value &value,idxNode *t) {
            //由于当前节点t已经在内存里，所以可以直接操作
            void *newNode;
            int i;
            for(i=0;i<t->t->len-1;i++){//查找x所在的子树
                if(key<t->key[i]) break;//到达第二层
            }
            if(t.type==0) {
                io.seekg(t->idx[i]);
                idxNode tmp;
                io.read(reinterpret_cast<char *> (&tmp),sizeof(idxNode));//打开下一个索引块
                newNode = insert (key, value, &tmp);//递归过程，直到下一层为数据块
            }
            else {
                io.seekg(t->idx[i]);
                dataNode tmp;
                io.read(reinterpret_cast<char *> (&tmp),sizeof(dataNode));//打开数据块
                newNode = insertData(key,value, &tmp);//添加数据
            }
            if(newNode==NULL) return NULL;
            else {              //添加索引项
                if(t->type==0) return addIdxBlk((idxNode *)newNode,t);
                else {return addDataBlk((dataNode *)newNode,t);}
            }
        }
        dataNode *insertData(const Key &key,const Value &value,dataNode *t){
            if(t->len<dataBlkSize){//x可以插入当前块中
                int i;
                for(i=t->len;i>0 && key<t->record[i-1];i--)
                    t->record[i]=t->record[i-1];
                t->record[i].first=key;
                t->record[i].second=value;
                (t->len)++;
                io.seekp(t->pos);
                io.write(reinterpret_cast<char *> (t),sizeof(idxNode));//把修改的数据块写回去,如果把idxNode改为*t应该也是对的
                return NULL;
            }

            //分裂当前块
            dataNode newNode;
            int i,j;
            int max = dataBlkSize/2;
            newNode.len=max+1;
            for(i=max,j=dataBlkSize-1;i>=0&&t->record[j]>key;--i)
                newNode.record[i]=t->record[j--];  //多出来的数据块放在了右边
            if(i>=0) {newNode.record[i--].first=key;newNode.record[i--].second=value;}
            for(;i>=0;i--) newNode.record[i]=t.record[j--];
            t->len=dataBlkSize - max;   //修改左块的大小
            if(j<t->len-1){         //x没有被插入到新数据块中
                for(;j>=0&&key<t->record[j];--j) t->record[j+1]=t->record[j];
                t->record[j+1].first=key;
                t->record[j+1].second=value;
            }

            //把修改后的两个节点写回文件
            if(t->nxt!=-1){//如果后面还有节点
                io.seekg(t->nxt);
                dataNode tmp;
                io.read(reinterpret_cast<char *> (&tmp), sizeof(dataNode));

                newNode.nxt=t->nxt;//多出来的数据块的下一个是原数据块的下一个
                io.seekp(0,std::ios::end);
                tmp.lst=t->nxt=newNode.pos=io.tellp();
                newNode.lst=t->pos; //要保存上一个结点的位置
                io.write(reinterpret_cast<char *> (&newNode),sizeof(dataNode));//把分裂后多出来的的数据块写回去
                io.seekp(t->pos);
                io.write(reinterpret_cast<char *> (t),sizeof(dataNode));//把分裂后的原数据块写回去
                io.seekp(t->nxt);
                io.write(reinterpret_cast<char *> (t),sizeof(dataNode));//把分裂后的原数据块的后一块写回去
            }
            else{//如果后面没有节点
                newNode.nxt=t->nxt;//多出来的数据块的下一个是原数据块的下一个
                io.seekp(0,std::ios::end);
                t->nxt=newNode.pos=io.tellp();
                newNode.lst=t->pos; //要保存上一个结点的位置
                io.write(reinterpret_cast<char *> (&newNode),sizeof(dataNode));//把分裂后多出来的的数据块写回去
                io.seekp(t->pos);
                io.write(reinterpret_cast<char *> (t),sizeof(dataNode));//把分裂后的原数据块写回去
            }
            return newNode;
        }
        //这里可以进行两次读入操作，把两个都读进来。
        idxNode *addIdxBlk(idxNode *newNode,idxNode *t){
            idxNode *p=newNode;

            //找新插入块的最小值存入min
            while(p->type==0){
                io.seekg(p->idx[0]);
                io.read(reinterpret_cast<char *> (p),sizeof(idxNode));
            }
            dataNode d;
            io.seekg(p->idx[0]);
            io.read(reinterpret_cast<char *> (&d),sizeof(dataNode));
            Key min=d.record[0];

            if(t->len<idxSize){         //索引快没有满，直接加入
                int i;
                for(i=t->len-1;i>0&&min<t->key[i-1];--i){
                    t->key[i]=t->key[i-1];
                    t->idx[i+1]=t->idx[i];
                }
                t->idx[i+1] =newNode;
                t->key[i]=min;
                ++(t->len);
                io.seekp(t->pos);
                io.write(reinterpret_cast<char *> (t),sizeof(idxNode));//把修改后的索引块写回去
                return NULL;
            }
            //分裂当前结点
            idxNode newIdx;
            newIdx.type=0;
            int max=idxSize/2;
            newIdx.len=max+1;
            int i,j;

            if(min>t->key[idxSize-2]){      //新插入的项是最大的，移到新索引块
                newIdx.key[max-1]=min;
                newIdx.idx[max]=newNode;
            }
            else{
                newIdx.key[max-1]=t->key[idxSize-2];
                newIdx.idx[max]=t->idx[idxSize-1];
                for(i=t->len-2;i>0&&min<t->key[i-1];--i){
                    t->key[i]=t->key[i-1];
                    t->idx[i+1]=t->idx[i];
                }
                t->key[i]=min;
                t->idx[i+1]=newNode.pos;
            }
            //分裂一半索引项到新增索引结点
            for(i=max-1,j=idxSize-1;i>0;--i,--j){
                newIdx.idx[i]=t->idx[i];
                newIdx.key[i-1]=t->key[j-1];
            }
            newIdx.idx[0]=t->idx[j];
            t->len=idxSize-max;
            io.seekp(t->pos);
            io.write(reinterpret_cast<char *> (t),sizeof(idxNode));//把分裂后的左索引块写回去
            io.seekp(0,std::ios::end);
            newIdx.pos=io.tellp();
            io.write(reinterpret_cast<char *> (&newIdx),sizeof(idxNode));//把分裂后的右索引块写回去
            return newIdx;
        }
        idxNode *addDataBlk(dataNode *newNode,idxNode *t){//newNode已经写入文件了，现在在操作上面一层（需要分裂）
            if(t->len<idxSize){//当前块还没有满，直接插入
                int i;
                for(i=t->len-1;i>0&&newNode->record[0]<t->key[i-1];--i){
                    t->key[i]=t->key[i-1];
                    t->idx[i+1] =t->idx[i];
                }
                t->key[i]=newNode ->record[0];
                t->idx[i+1] =newNode;
                ++(t->len);
                io.seekp(t->pos);
                io.write(reinterpret_cast<char *> (&newNode),sizeof(idxNode));//把修改后的索引块写回去
                return NULL;
            }
            //分裂结点(索引块)
            idxNode newIdx;
            newIdx.type=1;
            int max=idxSize/2;
            newIdx->len =max+1;
            int i,j;

            if(newNode.record[0] >t->key[idxSize-2]){//新增加的数据块是最大的
                newIdx.key[max-1] = newNode -> record[0];
                newIdx.idx[max]=newNode;
            }
            else {
                newIdx.key[max-1]=t->key[idxSize-2];
                newIdx .idx[max]=t->idx[idxSize-1];
                for(i=t->len-2;i>0&&newNode->record[0]<t->key[i-1];--i){
                    t->key[i]=t->key[i-1];
                    t->idx[i+1]=t->idx[i];
                }
                t->key[i]=newNode.record[0];
                t->idx[i+1]=newNode;
            }

            //将一半索引项移到新索引块
            for(i=max-1,j=idxSize-1;i>0;--i,--j){
                newIdx.idx[i]=t->idx[j];
                newIdx.key[i-1]=t->key[j-1];
            }
            newIdx.idx[0]=t->idx[j];
            t->len=idxSize - max;
            io.seekp(t->pos);
            io.write(reinterpret_cast<char *> (t),sizeof(idxNode));//把分裂后的左索引块写回去
            io.seekp(0,std::ios::end);
            newIdx.pos=io.tellp();
            io.write(reinterpret_cast<char *> (&newIdx),sizeof(idxNode));//把分裂后的左索引块写回去
            return newIdx;
        }
        void idxNode_copy(idxNode *t,const BTree& other){//这里的*t指向other中的将要被抄进来的结点
            io.seekp(t->pos);
            io.write(reinterpret_cast<char *> (&t),sizeof(idxNode));
            if(t->type==0){
                for(int i=0;i<t->len;i++) {
                    idxNode newNode;
                    std::fstream IO=other.io;
                    IO.seekg(t->idx[i]);
                    IO.read(reinterpret_cast<char *> (&newNode),sizeof(idxNode));//打开下一个索引块
                    idxNode_copy(&newNode,other);
                }
            }
            return ;
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
                io.open("file");
                if(!io) throw 1;
            }
            iterator(const iterator& other) {
                //  Copy Constructor
                pos=other.pos;
                idx=other.idx;
                len=other.len;
                io.open("file");
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
            // Todo Default
            ROOT=-1;
            io.open("file");
            io.seekp(0,std::ios::end);
            if(!io) throw 1;
            head.pos=io.tellp();
            head.len=0;//头结点不存放内容
            io.write(reinterpret_cast<char *> (&head),sizeof(dataNode));//头结点写入了文件
        }
        BTree(const BTree& other) {
            //  Copy
            io.open("file");
            if(!io) throw 1;
            ROOT=other.ROOT;

            //写头结点
            head.pos=other.head.pos;
            head.nxt=other.head.nxt;
            io.write(reinterpret_cast<char *> (&head),sizeof(dataNode));

            //写根节点
            idxNode root;
            std::fstream IO=other.io;
            IO.seekg(other.ROOT);
            IO.read(reinterpret_cast<char *> (&root),sizeof(idxNode));
            io.seekp(ROOT);
            io.write(reinterpret_cast<char *> (&root),sizeof(dataNode));

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
        }
        BTree& operator=(const BTree& other) {//和拷贝构造函数完全相同
            //  Assignment
            io.open("file");
            if(!io) throw 1;
            ROOT=other.ROOT;

            //写头结点
            head.pos=other.head.pos;
            head.nxt=other.head.nxt;
            io.write(reinterpret_cast<char *> (&head),sizeof(dataNode));

            //写根节点
            idxNode root;
            std::fstream IO=other.io;
            IO.seekg(other.ROOT);
            IO.read(reinterpret_cast<char *> (&root),sizeof(idxNode));
            io.seekp(ROOT);
            io.write(reinterpret_cast<char *> (&root),sizeof(dataNode));

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
        pair<iterator, OperationResult> insert(const Key& key, const Value& value) {
            //  insert function
            idxNode root;
            if(ROOT==-1){//空树的插入
                root.type=1;
                root.key[0]=key;//此处加了一个索引赋值操作

                //添加数据块
                dataNode p;
                p.record[0].first=key;
                p.record[0].second=value;
                io.seekp(0,std::ios::end);
                p.lst=head.pos;
                p.pos=io.tellp();
                io.write(reinterpret_cast<char *> (&p),sizeof(dataNode));//把数据块写回

                //修改头结点状态
                io.seekp(head.pos);
                head.nxt=p.pos;
                io.write(reinterpret_cast<char *> (&head),sizeof(dataNode));//把头结点写回

                //生成根
                root.idx[0]=io.tellp();
                io.seekp(0,std::ios::end);
                root.pos=io.tellp();
                io.write(reinterpret_cast<char *> (&root),sizeof(idxNode));//把生成的根写回
                ROOT=io.tellp();
                value_type k_v;
                k_v.first=key;
                k_v.second=value;
                return k_v;
            }
            io.seekg(ROOT);
            io.read(reinterpret_cast<char *> (&root),sizeof(idxNode));

            idxNode *p = insert(key,value,&root);//传入的是root的地址，相当于指向root的指针
            if(p!=NULL){    //原根节点被分裂了，处理树增高

                idxNode t;//t代表新的根
                t.type=0;
                t.len=2;
                t.idx[0]=ROOT;
                t.idx[1]=p->pos;

                io.seekp(0,std::ios::end);
                io.write(reinterpret_cast<char *> (p),sizeof(idxNode));//把新分裂出来索引块的写到文件末尾
                // 上面的写入操作是不是在别的函数里已经完成了

                //寻找第二块的最小值
                while(p->type==0) {
                    io.seekg(p->idx[0]);
                    io.read(reinterpret_cast<char *> (p),sizeof(idxNode));
                }
                io.seekg(p->idx[0]);
                dataNode d;
                io.read(reinterpret_cast<char *> (&d),sizeof(dataNode));//读出最小值所在的数据块

                t.key[0]=d->record[0];
                io.seekp(0,std::ios::end);
                io.write(reinterpret_cast<char *> (&t),sizeof(idxNode));//把新的根节点写回到文件末尾
            }
            value_type k_v;
            k_v.first=key;
            k_v.second=value;
            return k_v;
        }
        // Erase: Erase the Key-Value
        // Return Success if it is successfully erased
        // Return Fail if the key doesn't exist in the database
        OperationResult erase(const Key& key) {
            // TODO erase function
            return Fail;  // If you can't finish erase part, just remaining here.
        }
        // Return a iterator to the beginning
        iterator begin() {
            iterator it;
            it.pos=head.pos;//直接指向头节点
            return it;
        }
        const_iterator cbegin() const {
            iterator it;
            it.pos=head.pos;//直接指向头节点
            return it;
        }
        // Return a iterator to the end(the next element after the last)
        iterator end() {
            iterator it;
            dataNode cur;
            io.seekg(head.pos);
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
        const_iterator cend() const {
            iterator it;
            dataNode cur;
            io.seekg(head.pos);
            int w;
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
        bool empty() const {
            if(ROOT==-1) return 1;
            else return 0;
        }
        // Return the number of <K,V> pairs
        size_t size() const {
            dataNode cur;
            int cnt=0;
            io.seekg(head.pos);
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
            head.pos=-1;
        }
        // Return the value refer to the Key(key)
        Value at(const Key& key){
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
                if(i==tmp.len) throw "no match";
            }
            return tmp.record[i].second;
        }
        /**
         * Returns the number of elements with key
         *   that compares equivalent to the specified argument,
         * The default method of check the equivalence is !(a < b || b > a)
         */
        size_t count(const Key& key) const {
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
        const_iterator find(const Key& key) const {
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
        }
    };
}  // namespace sjtu
