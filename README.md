# saturn

This is a project for the RCI course in Instituto Superior Técnico - University of Lisbon.

There are two modules available:
..* service -- It's a server that first connects to a central server provided by the course and then joins a ring service.
The central server was provided by the course itself.
..* reqserv -- Client that requests a service by first contacting the central server.

To compile:
```
cd /path/to/saturn/
mkdir build
make
```

To run the service:
```    
/build/service –n id –j ip -u upt –t tpt [-i csip] [-p cspt] [-v] [-h]\n
```
where:
```
-n <service id>
-u <service udp port>
-t <service tcp port>
-i <central server's ip> -- not mandatory
-p <central server's port> -- not mandatory
-v -- print debug info during runtime -- not mandatory
-h -- show info -- not mandatory
```

## Scripts

The folder scripts contains a script to create multiple scripts that join almost at the same time and leave also almost at the same time, in order to test if the service is able to handle it.
