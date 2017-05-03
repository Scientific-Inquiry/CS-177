// Simulation of the Hertz airport shuttle bus, which picks up passengers
// from the airport terminal building going to the Hertz rental car lot.

#include <iostream>
#include <string.h>
#include <ctime>
#include <cstdio>
#include <cmath>
#include <queue>
#include "cpp.h"
using namespace std;

#define NUM_TERMNL 4
#define NUM_SEATS 6      // number of seats available on shuttle
#define TINY 1.e-20      // a very small time period
//update: changed number of terminals
//#define TERMNL 0         // named constants for labelling event set
int const TERMNL0 = 0, TERMNL1 = 1, TERMNL2 = 2, TERMNL3 = 3, CARLOT = 4;
//#define CARLOT 1

//facility_set buttons ("Curb",2);  // customer queues at each stop
facility_set buttons ("Curb",5);  // customer queues at each stop

facility rest ("rest");           // dummy facility indicating an idle shuttle
event_set get_off("get off", 5);
event_set hop_on("board shuttle", 5);  // invite one customer to board at this stop
event boarded ("boarded");             // one customer responds after taking a seat

//event_set shuttle_called ("call button", 2); // call buttons at each location
event_set shuttle_called ("call button", 5); // call buttons at each location

queue<int> next_termnl;

void make_passengers(long whereami);       // passenger generator
//string places[2] = {"Terminal", "CarLot"}; // where to generate
string places[5] = {"Terminal 1", "Terminal 2", "Terminal 3", "Terminal 4","CarLot"}; // where to generate
long group_size();

void passenger(long whoami, int dest);                // passenger trajectory
string people[2] = {"arr_cust","dep_cust"}; // who was generated

void shuttle();                  // trajectory of the shuttle bus consists of...
void loop_around_airport(long & seats_used);      // ... repeated trips around airport
void load_shuttle(long whereami, long & on_board); // posssibly loading passengers
qtable shuttle_occ("bus occupancy");  // time average of how full is the bus

extern "C" void sim()      // main process
{
  srand(time(NULL));
  create("sim");
  shuttle_occ.add_histogram(NUM_SEATS+1,0,NUM_SEATS);
  //cout<<"before making all passengars\n";
  //max_events(1000000000);
  make_passengers(TERMNL0);  // generate a stream of arriving customers
  make_passengers(TERMNL1);  // generate a stream of arriving customers
  make_passengers(TERMNL2);  // generate a stream of arriving customers
  make_passengers(TERMNL3);  // generate a stream of arriving customers
  make_passengers(CARLOT);  // generate a stream of departing customers
  shuttle();                // create a single shuttle
  hold (1440);              // wait for a whole day (in minutes) to pass
  report();
  status_facilities();
}

// Model segment 1: generate groups of new passengers at specified location

void make_passengers(long whereami)
{
  const char* myName=places[whereami].c_str(); // hack because CSIM wants a char*
  create(myName);
//  cout<<"make: whereami = "<<whereami<<endl;
  while(clock < 1440.)          // run for one day (in minutes)
  {
    if(whereami == CARLOT){
    	hold(expntl(10));           // exponential interarrivals, mean 10 minutes
    }
    else{
    	hold(expntl(10*NUM_TERMNL));           // exponential interarrivals, mean 10 minutes
    }
    long group = group_size();
    int dest;

    if(whereami == 4)
    	dest = uniform(0,3);
    else
	dest = 4;

    for (long i=0;i<group;i++)  // create each member of the group
      passenger(whereami, dest);      // new passenger appears at this location
  }
}
// Model segment 2: activities followed by an individual passenger

void passenger(long whoami, int dest)
{
  const char* myName;
  //cout<<"choosing type of person\n";
  if(whoami < 4){
    // cout<<"arriving\n" ;
     myName = people[0].c_str(); // hack because CSIM wants a char*
  }
  else{
    // cout<<"departing\n";
     myName = people[1].c_str(); // hack because CSIM wants a char*
  }
  create(myName);
  //cout<<"passenger: whoami = "<<whoami<<endl;
  buttons[whoami].reserve();     // join the queue at my starting location
  //cout<<"after buttons\n";
  shuttle_called[whoami].set();  // head of queue, so call shuttle
  //cout<<"after calling shuttle\n";
  hop_on[whoami].queue();        // wait for shuttle and invitation to board
  shuttle_called[whoami].clear();// cancel my call; next in line will push 
  hold(uniform(0.5,1.0));        // takes time to get seated
  boarded.set();                 // tell driver you are in your seat
  buttons[whoami].release();     // let next person (if any) access button
 // cout<<"before waiting to get off\n"<<endl;
  get_off[dest].wait();
 // cout<<"get_off waiting: "<<get_off[0].wait_cnt()<<endl;
 // cout<<"get_off waiting: "<<get_off[1].wait_cnt()<<endl;
 // cout<<"get_off waiting: "<<get_off[2].wait_cnt()<<endl;
 // cout<<"get_off waiting: "<<get_off[3].wait_cnt()<<endl;
  //get_off_now.wait();            // everybody off when shuttle reaches next stop
}

// Model segment 3: the shuttle bus

void shuttle() {
  create ("shuttle");
  while(1) {  // loop forever
    // start off in idle state, waiting for the first call...
    rest.reserve();                   // relax at garage till called from somewhere
    long who_pushed = shuttle_called.wait_any();
    shuttle_called[who_pushed].set(); // loop exit needs to see event
    rest.release();                   // and back to work we go!

    long seats_used = 0;              // shuttle is initially empty
    shuttle_occ.note_value(seats_used);

    hold(2);  // 2 minutes to reach car lot stop

    // Keep going around the loop until there are no calls waiting
    while ((shuttle_called[TERMNL0].state()==OCC)||(shuttle_called[TERMNL1].state()==OCC)||
	   (shuttle_called[TERMNL2].state()==OCC) || (shuttle_called[TERMNL3].state()==OCC) ||
           (shuttle_called[CARLOT].state()==OCC)  )
      loop_around_airport(seats_used);
  }
}

long group_size() {  // calculates the number of passengers in a group
  double x = prob();
  if (x < 0.3) return 1;
  else {
    if (x < 0.7) return 2;
    else return 5;
  }
}

void loop_around_airport(long & seats_used) { // one trip around the airport
  int prev_length = 0;
  // Start by picking up departing passengers at car lot
  load_shuttle(CARLOT, seats_used);
  shuttle_occ.note_value(seats_used);

  hold (uniform(3,5));  // drive to airport terminal

  // drop off all departing passengers at airport terminal
 // cout<<"\nCarlot: seats used = "<<seats_used<<" | \n";
  if(seats_used > 0 ) {
      prev_length = get_off[TERMNL0].wait_cnt();
      get_off[TERMNL0].set();
      seats_used = seats_used - (prev_length); //- get_off[TERMNL0].wait_cnt());
   //   cout<<"New Seat:"<<seats_used<<" | prev_length = "<<prev_length<<" | new length = "<<get_off[0].wait_cnt()<<endl; 
      shuttle_occ.note_value(seats_used);
  }

  // pick up arriving passengers at airport terminal
  if(next_termnl.empty() || next_termnl.front() == TERMNL0){
  	load_shuttle(TERMNL0, seats_used);
 	shuttle_occ.note_value(seats_used);
	if(!next_termnl.empty())
		next_termnl.pop();
  }

  hold (uniform(3,5));  // drive to Hertz car lot

   //drop off
  //cout<<"\nTerm1: seats used = "<<seats_used<<" | \n";
   if(seats_used > 0 ) {
      
      prev_length = get_off[TERMNL1].wait_cnt();
      get_off[TERMNL1].set();
      seats_used = seats_used -(prev_length);// - get_off[TERMNL1].wait_cnt());
    //  cout<<"New Seat:"<<seats_used<<" | prev_length = "<<prev_length<<" | new length = "<<get_off[1].wait_cnt()<<endl; 
      shuttle_occ.note_value(seats_used);
  }
  // pick up arriving passengers at airport terminal
  if(next_termnl.empty() || next_termnl.front() == TERMNL1){
  	load_shuttle(TERMNL1, seats_used);
  	shuttle_occ.note_value(seats_used);
	if(!next_termnl.empty())
		next_termnl.pop();
  }

  hold (uniform(3,5));  // drive to Hertz car lot
 
  //drop off
  //cout<<"\nTerm2: seats used = "<<seats_used<<" | \n";
 if(seats_used > 0 ) {
	prev_length = get_off[TERMNL2].wait_cnt();
      get_off[TERMNL2].set();
      seats_used = seats_used- (prev_length); //- get_off[TERMNL2].wait_cnt());
    //  cout<<"New Seat:"<<seats_used<<" | prev_length = "<<prev_length<<" | new length = "<<get_off[2].wait_cnt()<<endl; 
      shuttle_occ.note_value(seats_used);
    next_termnl.push(TERMNL2);
 }


  //pick up arriving passengers at airport terminal
  if(next_termnl.empty() || next_termnl.front() == TERMNL2){
  	load_shuttle(TERMNL2, seats_used);
  	shuttle_occ.note_value(seats_used);
	if(!next_termnl.empty())
		next_termnl.pop();
  }

  hold (uniform(3,5));  // drive to Hertz car lot
   
   //drop off
 // cout<<"\nTerm3: seats used = "<<seats_used<<" | \n";
   if(seats_used > 0) {
	prev_length = get_off[TERMNL3].wait_cnt();
      
      get_off[TERMNL3].set();
      seats_used = seats_used -(prev_length); //- get_off[TERMNL3].wait_cnt());
   //   cout<<"New Seat:"<<seats_used<<" | prev_length = "<<prev_length<<" | new length = "<<get_off[3].wait_cnt()<<endl; 
      shuttle_occ.note_value(seats_used);
    }

  // pick up arriving passengers at airport terminal
  if(next_termnl.empty() || next_termnl.front() == TERMNL3){
  	load_shuttle(TERMNL3, seats_used);
  	shuttle_occ.note_value(seats_used);
	if(!next_termnl.empty())
		next_termnl.pop();
  }

  hold (uniform(3,5));  // drive to Hertz car lot
  //CAR LOT drop off 
  //cout<<"\nTerm4: seats used = "<<seats_used<<" | \n";
   if(seats_used > 0) {
      prev_length = get_off[CARLOT].wait_cnt();
      get_off[CARLOT].set();
      seats_used = seats_used- (prev_length); //- get_off[CARLOT].wait_cnt());
     // cout<<"New Seat:"<<seats_used<<" | prev_length = "<<prev_length<<" | new length = "<<get_off[4].wait_cnt()<<endl; 
      shuttle_occ.note_value(seats_used);
   }
   seats_used = 0;
  // pick up arriving passengers at airport terminal
 // load_shuttle(TERMNL3, seats_used);
  shuttle_occ.note_value(seats_used);
  hold (uniform(3,5));  // drive to Hertz car lot
  

  // Back to starting point. Bus is empty. Maybe I can rest...
}

void load_shuttle(long whereami, long & on_board)  // manage passenger loading
{
  if(on_board >= NUM_SEATS){
	next_termnl.push(whereami);
  }
  // invite passengers to enter, one at a time, until all seats are full
  while((on_board < NUM_SEATS) &&
    (buttons[whereami].num_busy() + buttons[whereami].qlength() > 0))
  {
    //cout<<"boarding\n";
    hop_on[whereami].set();// invite one person to board
    boarded.wait();  // pause until that person is seated
    on_board++;
    //cout<<"seats used = "<<on_board<<endl;
    hold(TINY);  // let next passenger (if any) reset the button
  }
}
