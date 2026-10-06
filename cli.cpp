#include <iostream>
#include <sstream>
#include <string>

#include "orderbook.h"

int main(){
    OrderBook book;
    uint64_t next_id = 1;

    while(true){
        cout << "< ";
        std::string line;
        if(!std::getline(std::cin,line)) break;
        if(line.empty()) continue;
        std::string command;
        std::string str_side;
        int64_t price;
        int64_t quantity;
        uint64_t id =1;
        uint64_t user_id;
        size_t n;
        std::stringstream ss(line);
        ss >> command;

        if(command == "ADD"){
            ss >> user_id >> str_side >> price >> quantity;
            Order o{id++,user_id,price,quantity,0,str_side,Ordertype::Limit};
            book.addOrder(o);
        }else if(command == "MARKET"){
            ss >> user_id >> str_side >> price >> quantity;       
            Order o{id++,user_id,price,quantity,0,str_side,Ordertype::Limit};
            book.addOrder(o);

        }else if(command == "CANCEL"){
            ss >> id;
        }else if(command == "MODIFY"){
            ss >> id >> pre >> quan;
        }else if(command == "DEPTH"){
            ss >> n;
        }else if(command == "TRADES"){
            
        }else{
          
        }







    }
}
