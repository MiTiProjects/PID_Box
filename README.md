# Temperature control of a metal box - PID
The project aim is to study various starategies to regulate the temperature of a prototype oven. 
In particular, a PID algorithm was implemented and its performances were compared to an ON/OFF algorithm. 
Both algorithms were implemented into a c++ script. The prototype oven consist of a resistor and a transistor enclosed in a metal box.

![alt text](https://github.com/MiTiProjects/PID_Box/blob/main/setup.png)

The primary emphasis was on calibrating the parameters of the PID algorithm.

The dynamic of the system was also translated from the thermodynamic domain to the electrical one. 
In particular, it is possible to represent temperature by a potential difference, thermal power by an 
electric current, heat capacity with a capacitor and finally thermal conductivity through a resistance.
