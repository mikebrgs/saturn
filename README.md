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

# MIT License

Copyright (c) 2018 Miguel Rego Borges

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
