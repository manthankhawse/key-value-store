#include <bits/stdc++.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
using namespace std;

static const int MAX_LEN = 4096;

struct Client {
    int fd = -1;
    bool connect_to(const char* ip, uint16_t port) {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return false;
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &addr.sin_addr);
        return ::connect(fd,(sockaddr*)&addr,sizeof(addr))==0;
    }
    bool write_full(const void* buf, size_t len) {
        const char* p=(const char*)buf;
        while(len>0){
            ssize_t n=write(fd,p,len);
            if(n<=0) return false;
            p+=n; len-=n;
        }
        return true;
    }
    bool read_full(void* buf,size_t len){
        char* p=(char*)buf;
        while(len>0){
            ssize_t n=read(fd,p,len);
            if(n<=0) return false;
            p+=n; len-=n;
        }
        return true;
    }
    bool send_cmd(const string& msg){
        uint32_t len=msg.size();
        char wbuf[4+MAX_LEN];
        memcpy(wbuf,&len,4);
        memcpy(wbuf+4,msg.data(),len);
        if(!write_full(wbuf,4+len)) return false;
        uint32_t rlen=0;
        if(!read_full(&rlen,4)) return false;
        if(rlen>MAX_LEN) return false;
        static char rbuf[MAX_LEN];
        if(!read_full(rbuf,rlen)) return false;
        return true;
    }
};

uint64_t now_us(){
    auto t=chrono::high_resolution_clock::now();
    return chrono::duration_cast<chrono::microseconds>(t.time_since_epoch()).count();
}

int main(int argc,char** argv){
    int clients=1;
    long long ops=100000;
    long long keyspace=100000;
    string mode="mixed";

    for(int i=1;i<argc;i++){
        string a=argv[i];
        if(a.rfind("--clients=",0)==0) clients=stoi(a.substr(10));
        if(a.rfind("--ops=",0)==0) ops=stoll(a.substr(6));
        if(a.rfind("--keyspace=",0)==0) keyspace=stoll(a.substr(11));
        if(a.rfind("--mode=",0)==0) mode=a.substr(7);
    }

    vector<thread> th;
    vector<vector<uint32_t>> samples(clients);
    atomic<long long> done{0};
    atomic<long long> errors{0};

    auto worker=[&](int tid){
        Client c;
        if(!c.connect_to("127.0.0.1",1234)){
            errors++;
            return;
        }
        mt19937_64 rng(tid+123);
        for(;;){
            long long cur=done.fetch_add(1);
            if(cur>=ops) break;

            long long k = rng()%keyspace;
            string key="k"+to_string(k);
            string cmd;
            if(mode=="get") cmd="GET "+key;
            else if(mode=="set"){
                string val="v"+to_string(rng()%1000000);
                cmd="SET "+key+" "+val;
            } else {
                if(rng()%2){
                    string val="v"+to_string(rng()%1000000);
                    cmd="SET "+key+" "+val;
                } else {
                    cmd="GET "+key;
                }
            }

            uint64_t t0=now_us();
            if(!c.send_cmd(cmd)){
                errors++;
                continue;
            }
            uint64_t t1=now_us();
            samples[tid].push_back(t1-t0);
        }
    };

    auto tstart=now_us();
    for(int i=0;i<clients;i++) th.emplace_back(worker,i);
    for(auto &x:th) x.join();
    auto tstop=now_us();

    vector<uint32_t> all;
    for(auto &v:samples) all.insert(all.end(),v.begin(),v.end());
    sort(all.begin(),all.end());

    double sec=(tstop-tstart)/1e6;
    double thr=done.load()/sec;

    auto pct=[&](double p){
        if(all.empty()) return 0u;
        size_t idx=p*all.size();
        if(idx>=all.size()) idx=all.size()-1;
        return all[idx];
    };

    cout<<"Total ops: "<<done.load()<<"\n";
    cout<<"Errors: "<<errors.load()<<"\n";
    cout<<"Time: "<<sec<<" sec\n";
    cout<<"Throughput: "<<thr<<" ops/sec\n";
    if(!all.empty()){
        cout<<"p50: "<<pct(0.50)<<" us\n";
        cout<<"p95: "<<pct(0.95)<<" us\n";
        cout<<"p99: "<<pct(0.99)<<" us\n";
    }
}
