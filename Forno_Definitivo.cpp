// Elettronica ed acquisizione dati
// Francesca Rossi e Mirco Tincani
// anno 2016/2017

#include "stdafx.h"
#include <iostream>
#include <conio.h> // permette l'uscita da un ciclo infinito premendo un tasto
#include <cmath>
#include <iomanip>
#include <string>
#include <fstream>
#include <cstdlib>
#include <windows.h> // per l'uso di Sleep(int ms)
#include <NIDAQmx.h> // permette l'uso della libreria NIDAQmx
using namespace std;

//COSTANTI:
 // [ALLARME] Temperatura massima 
  #define TMAX 115
  #define TMIN 0
  #define SMAX 10
  #define SMIN 0
 // [FILTRO] Numero di campioni da mediare 
  #define N_MED 5
  #define N_MED_PWM 2
  #define N_Der 6

 //LAYOUT
#define sd 1


void PWMout(TaskHandle* phandle, float64 Vout)
{
// Gestione rapida dei casi limite

	if (Vout < 0.001)
		Vout = 0.001;

	if (Vout > 0.9999)
		Vout = 0.9999;

	ErrorCheck(DAQmxStopTask(*phandle));
	ErrorCheck(DAQmxSetCOPulseDutyCyc(*phandle, "Dev1/ctr0", Vout));
	ErrorCheck(DAQmxStartTask(*phandle));
}



int main(void)
{
    
//--DICHIARAZIONE VARIABILI

   //Parametri PID  [Input Utente]
	double kp, ki, kd;
   // Tensioni  risultante azione proporzionale derivativa
    double Prop=0, Int=0, Der=0;
	
   //parametri del Pt100 [sensore di temperatura]
	double c0=-0.125079, c1=1.04063, c2=1.69477e-3, c3=9.57745e-6;
	
   //Frequenza di Campionamento
	int f_camp=50;
    
   // Temperatura: 
   //  - T0=   Temperatura Iniziale		
   //  - Tset= Temperatura Set Point  [Input Utente]
   //  - Tmis= Temperatura Campionata 
   //  - Tmed= Temperatura Media
   //  - Tprec=Temperatura media Precedente alla corrente
   //  - Soglia = differenza tra le temperature di soglia per l'accensione e spegnimento del forno 
	double T0=0, Tset=0, Tmis=0, Tmed=0, Tprec=0, err, errMed, errMedP, soglia=1.0;
	
   // Tensione:
   //	- V0=	 Tensione Iniziale
   //	- Vin= 	 Tensione misurata in [V]
   //	- VinmV= Tensione misurata in [mV]
   //	- Vout=	 Tensione in uscita
	double V0=0, Vin=0, VinmV=0, temp=0;
	float64 Vout;
	double vettorT[N_MED]={0},  errDer[N_Der]={0}, sum=0, sumT=0;
	
   //VARIABILI DI CONTROLLO E AUSILIARIE
	int control = 0;
	string NomeFile;
	int scelta, opzione, i, flagSoglia=0, c=0;
	ofstream fileout;//dichiaro un file
	

   
//--INIZIALIZZAZIONE OI SCHEDA DI ACQUISIZIONE
	
	cout<<"-- CONTROLLO DELLA TEMPERATURA  --"<< endl<<endl;
	 
  //OutPut [alimentazione]
	TaskHandle alimentazione;
	DAQmxCreateTask("", &alimentazione);
	DAQmxCreateAOVoltageChan(alimentazione, "Dev1/ao0", "", 0.0, 10.0, DAQmx_Val_Volts, NULL);
	DAQmxStartTask(alimentazione);
	Vout=0;
	DAQmxWriteAnalogScalarF64(alimentazione, 1, 10.0, Vout, NULL);
  //InPut [misurazione]
    TaskHandle lettura;
	DAQmxCreateTask("", &lettura);
	DAQmxCreateAIVoltageChan(lettura, "Dev1/ai0", "", DAQmx_Val_Cfg_Default, 0.0, 10.0, DAQmx_Val_Volts, "");
	DAQmxStartTask(lettura);


  //PWM
	cout << "PWM ON" << endl;

	TaskHandle hTaskHandlePWM;

	ErrorCheck(DAQmxCreateTask("", &hTaskHandlePWM));
	ErrorCheck(DAQmxCreateCOPulseChanFreq(hTaskHandlePWM, "Dev1/ctr0", "", DAQmx_Val_Hz, DAQmx_Val_Low, 0.0, 1000.0, 0.5));
	ErrorCheck(DAQmxCfgImplicitTiming(hTaskHandlePWM, DAQmx_Val_ContSamps, 1));
	ErrorCheck(DAQmxStopTask(hTaskHandlePWM));

	 
//Definizione della Frequenza di Campionamento
	cout <<"La frequenza di campionamento e' di " <<f_camp<<" ms"<<endl;
	cout <<"Si desidera cambiare questo valore ? (SI=1,NO=0)";
	cin >>scelta;
    if (scelta==1)
	{
		cout <<"\nInserire il nuovo valore : "; 
		cin >>f_camp;
	}
	
//Inserimento della Temperatura di SetPoint
	do
    {
       cout<<"\nInserire il valore della temperatura di setpoint ("<< TMIN << "-" <<TMAX << ")"<<endl;
	   cin>>Tset;
	}
    while(Tset>TMAX||Tset<TMIN);
	 
//SAVE FILE - Preparazione file del salvataggio temperatura su file di testo
	cout <<"\nSi desiderano salvare su file di testo i valori assunti dalla temperatura? (1/0)";
	cin >>scelta;
	getchar(); // serve per eliminare l'invio floating
	if (scelta==1)
	{
		do
		{
			cout <<"\nNome File (*.txt): ";
			getline(cin, NomeFile);
			fileout.open(NomeFile.c_str());
			if (!fileout)
				{
					cerr << "Error opening temperatura.txt" << endl;
				}
		}
		while(!fileout);
	}
	fileout.close();
	
//MENU - Selezionare la modalita' 
	cout<<"\n\nCon quale modalita'  si vuole regolare la temperatura?"<< endl;
	cout<<"1-ON/OFF Soglia"<<endl;  
	cout<<"2-PID Analogico"<<endl;
	cout<<"3-PID PWM"<<endl;
	cin>> opzione; 

//--OPZIONE 1----------------------------------------------------------------
  //ON/OFF
	if(opzione==1)
	{
		cout<<"-->  CONTROLLO ON/OFF"<<endl<<endl;
		do
		{
			cout<<"\nDefinire la soglia ("<< SMIN << "-" <<SMAX << ")"<<endl;
			cin>>soglia;
		}
		while(soglia>SMAX||soglia<SMIN);
		
	//Controllo ON/OFF
		flagSoglia=0;
		while(control!=27)
		{
			if(_kbhit())
			{
				Vout=0;
				DAQmxWriteAnalogScalarF64(alimentazione, 1, 10.0, Vout, NULL);
				cout<<"ARRESTO "<<endl;
			//Spegnimento strumento
				DAQmxStopTask(alimentazione);
				DAQmxStopTask(lettura);
				DAQmxClearTask(alimentazione);
				DAQmxClearTask(lettura);
				getchar();
				break;
			}

			sumT=0;
			for(i=0;i<N_MED;i++)
        	{
            //Misurazione della temperatura del forno
				DAQmxReadAnalogScalarF64(lettura, 10.0, &VinmV, 0);
            	Vin=VinmV*1000; //Tensione in mV
            	
            //Conversione tensione-temperatura
            	Tmis=c0+c1*Vin+c2*Vin*Vin+c3*Vin*Vin*Vin; 
			//ARRESTO EMERGENZA - Controllo temperatura Max
				if(Tmis>=TMAX)
				{
					Vout=0;
					DAQmxWriteAnalogScalarF64(alimentazione, 1, 10.0, Vout, NULL);
					cout<<"ARRESTO EMERGENZA:\n\t\t sovrariscaldamento"<<endl;
				//Spegnimento strumento
					DAQmxStopTask(alimentazione);
					DAQmxStopTask(lettura);
					DAQmxClearTask(alimentazione);
					DAQmxClearTask(lettura);
					getchar();
					break;
				}

            	sumT=sumT+Tmis;
            	
            //Tempo che intercorre tra un ciclo e l'altro
            	Sleep(f_camp);

	         }

             Tmed=sumT/N_MED;
			         
		//CONTROLLO - 	
        	if(Tmed>=Tset+soglia)
        		{
        		//Invio Tensione all'attuatore
        			Vout=0;
					DAQmxWriteAnalogScalarF64(alimentazione, 1, 10.0, Vout, NULL);
					flagSoglia=1;
					cout<<"OFF";
        		}
            if(Tmed<=Tset-soglia) 
            	{
        		//Invio Tensione all'attuatore
        			Vout=10.0;
					DAQmxWriteAnalogScalarF64(alimentazione, 1, 10.0, Vout, NULL);
					flagSoglia=0;
					cout<<"ON";
        		}
        	if(Tmed>Tset-soglia&&Tmed<Tset+soglia&&flagSoglia==0)
            	{
        		//Invio Tensione all'attuatore
        			Vout=10.0;
					DAQmxWriteAnalogScalarF64(alimentazione, 1, 10.0, Vout, NULL);
					cout<<"ON";
        		}
        	if(Tmed>Tset-soglia&&Tmed<Tset+soglia&&flagSoglia==1)
            	{
        		//Invio Tensione all'attuatore
        			Vout=0;
					DAQmxWriteAnalogScalarF64(alimentazione, 1, 10.0, Vout, NULL);
					cout<<"OFF";
        		}
			cout<<"\t -->\t"<<Tmed<<"\tC"<< "\t\t\tTset= "<<Tset<<"C"<<endl;
			
		//Scrittura su file
        	fileout.open(NomeFile.c_str(), std::fstream::app); // ...app: APPend, concatena a file esistente
        	fileout<< Tmed <<","<<Vout<<endl;
        	fileout.close();

		}
	}
	 
//--OPZIONE 2----------------------------------------------------------------
	if(opzione==2)
    {
    			cout<<"--> CONTROLLO PID  ANALOGICO"<<endl<<endl;
    	
    //Definizione dei parametri di PID
    	cout <<"Digitare il valore Kp che si vuole utilizzare"<< endl;
    	cin>> kp;
    	cout <<"Digitare il valore Ki che si vuole utilizzare"<< endl;
    	cin>> ki;
    	cout <<"Digitare il valore Kd che si vuole utilizzare"<< endl;
    	cin>> kd;
		
    	
    //Misurazione temperatura iniziale [V0]
    	DAQmxReadAnalogScalarF64(lettura, 10.0, &V0, 0);
    	V0=V0*1000; //Tensione in mV

    //Conversione tensione-temperatura 
    	T0=c0+c1*V0+c2*V0*V0+c3*V0*V0*V0; 
    	
    	errMedP=Tset-T0;

      //Inizializzo il vettore per calcolare le derivate 
		for(i=0;i<N_Der;i++)
		{
			errDer[i]=errMedP;
		}

	    while(control!=1)
        {
			if(_kbhit())
			{
				Vout=0;
				DAQmxWriteAnalogScalarF64(alimentazione, 1, 10.0, Vout, NULL);
				cout<<"ARRESTO "<<endl;
			//Spegnimento strumento
				DAQmxStopTask(alimentazione);
				DAQmxStopTask(lettura);
				DAQmxClearTask(alimentazione);
				DAQmxClearTask(lettura);
				getchar();
				break;
			}
        //Calcolo Temperatura Media
        	sumT=0;
        	for(i=0;i<N_MED;i++)
        	{
            //Misurazione della temperatura del forno
				DAQmxReadAnalogScalarF64(lettura, 10.0, &VinmV, 0);
            	Vin=VinmV*1000; //Tensione in mV
            	
            //Conversione tensione-temperatura
            	Tmis=c0+c1*Vin+c2*Vin*Vin+c3*Vin*Vin*Vin; 

			//ARRESTO EMERGENZA - Controllo temperatura Max
				if(Tmis>=TMAX)
				{	
					Vout=0;
					DAQmxWriteAnalogScalarF64(alimentazione, 1, 10.0, Vout, NULL);
					cout<<"ARRESTO EMERGENZA:\n\t\t sovrariscaldamento"<<endl;
				//Spegnimento strumento
					DAQmxStopTask(alimentazione);
					DAQmxStopTask(lettura);
					DAQmxClearTask(alimentazione);
					DAQmxClearTask(lettura);
					break;
				}
            	sumT=sumT+Tmis;
            	
            //Tempo che intercorre tra un ciclo e l'altro
            	Sleep(f_camp);

	         }

             Tmed=sumT/N_MED;
             
        //Scrittura su file
        	fileout.open(NomeFile.c_str(), std::fstream::app); // ...app: APPend, concatena a file esistente
        	fileout<< Tmed <<","<<Vout<<endl;
        	fileout.close();
        	
	        errMed=Tset-Tmed;

			for(i=0;i<N_Der-1;i++)
			{
				errDer[i]=errDer[i+1];
			} 
			errDer[N_Der-1]=errMed;

		//PID - Calcolo delle Tensioni
            // Proporzionale
             Prop=kp * errMed;
					
            //Derivativo
			 if (c<=N_Der)
			 {
				Der=0.0;	
			 }
			 else
			 {
				Der= kd * ( (errDer[N_Der-1]-errDer[0])/(f_camp*N_MED*N_Der*0.001) );
			 }
         	
			 //N_MED -- con metodo dei trapezi
	         Int= Int+ ki * ((errMed+errMedP)*(f_camp*N_MED*0.001)/2);

			 
			 printf("Kp: %.2f\tKi: %.2f\tKd: %.2f",Prop,Int,Der);

		//ALIMENTAZIONE
			 Vout=Prop+Der+Int;
			 cout<<Tmed<<"\tC"<<;
		  //Controllo Tensione
		  	if(Vout<=0)
			{
				Vout=0; 				
				cout <<"OFF"<< endl; 	
			}			 	

			else
			{
				if(Vout>=10.0)
				{
					Vout=10.0; 
					for(i=0;i<int(Vout)*sd;i++)
					{
						cout <<char(178);
					}	 	
					cout<<"SAT!"<<endl;
				}
				else
				{
					for(i=0;i<int(Vout)*sd;i++)
					{
						cout <<char(178);
					}
					cout<<endl;
				}
			}
		    
		//Invio Tensione all'attuatore
			DAQmxWriteAnalogScalarF64(alimentazione, 1, 10.0, Vout, NULL);
		
		//Scrittura su file
        	fileout.open(NomeFile.c_str(), std::fstream::app); // ...app: APPend
        	fileout<< Tmed <<","<<Vout<<endl;
        	fileout.close();

			
		//Inizializzo Errore Precedente
			 errMedP=errMed;

		c++;
		}
	}
	
//--OPZIONE 3----------------------------------------------------------------	
	if(opzione==3)
	{
		    	cout<<"--> CONTROLLO PID PWM"<<endl<<endl;
    	
    //Definizione dei parametri di PID
    	cout <<"Digitare il valore Kp che si vuole utilizzare"<< endl;
    	cin>> kp;
    	cout <<"Digitare il valore Ki che si vuole utilizzare"<< endl;
    	cin>> ki;
    	cout <<"Digitare il valore Kd che si vuole utilizzare"<< endl;
    	cin>> kd;
		
    	
    //Misurazione temperatura iniziale [V0]
    	DAQmxReadAnalogScalarF64(lettura, 10.0, &V0, 0);
    	V0=V0*1000; //Tensione in mV

    //Conversione tensione-temperatura 
    	T0=c0+c1*V0+c2*V0*V0+c3*V0*V0*V0; 
    
		c=0;
    	errMedP=Tset-T0;

      //Inizializzo il vettore per calcolare le derivate 
		for(i=0;i<N_Der;i++)
		{
			errDer[i]=errMedP;
		}

	    while(control!=1)
        {
			if(_kbhit())
			{
				Vout=0;
				DAQmxWriteAnalogScalarF64(alimentazione, 1, 10.0, Vout, NULL);
				cout<<"ARRESTO "<<endl;
			//Spegnimento strumento
				DAQmxStopTask(lettura);
				DAQmxClearTask(lettura);
				ErrorCheck(DAQmxStopTask(hTaskHandlePWM));
				ErrorCheck(DAQmxClearTask(hTaskHandlePWM));
				getchar();
				break;
			}
        //Calcolo Temperatura Media
        	sumT=0;
        	for(i=0;i<N_MED_PWM;i++)
        	{
            //Misurazione della temperatura del forno
				DAQmxReadAnalogScalarF64(lettura, 10.0, &VinmV, 0);
            	Vin=VinmV*1000; //Tensione in mV
            	
            //Conversione tensione-temperatura
            	Tmis=c0+c1*Vin+c2*Vin*Vin+c3*Vin*Vin*Vin; 

			//ARRESTO EMERGENZA - Controllo temperatura Max
				if(Tmis>=TMAX)
				{
					Vout=0;
					DAQmxWriteAnalogScalarF64(alimentazione, 1, 10.0, Vout, NULL);
					cout<<"ARRESTO EMERGENZA:\n\t\t sovrariscaldamento"<<endl;
				//Spegnimento strumento
					DAQmxStopTask(lettura);
					DAQmxClearTask(lettura);
					ErrorCheck(DAQmxStopTask(hTaskHandlePWM));
					ErrorCheck(DAQmxClearTask(hTaskHandlePWM));

					break;
				}
            	sumT=sumT+Tmis;
            	
            //Tempo che intercorre tra un ciclo e l'altro
            	Sleep(f_camp);

	         }

             Tmed=sumT/N_MED_PWM;
                     	
	         errMed=Tset-Tmed;
			 for(i=0;i<N_Der-1;i++)
			 {
				 errDer[i]=errDer[i+1];
			 } 
			 errDer[N_Der-1]=errMed;

	         
		//PID - Calcolo delle Tensioni
            // Proporzionale
             Prop=kp * errMed;
					
            //Derivativo
			 if (c<=N_Der)
			 {
				Der=0.0;	
			 }
			 else
			 {
				Der= kd * ( (errDer[N_Der-1]-errDer[0])/(f_camp*N_MED_PWM*N_Der*0.001) );
			 }
         	
			 //N_MED_PWM -- con metodo dei trapezi
	         Int= Int+ ki * ((errMed+errMedP)*(f_camp*N_MED_PWM*0.001)/2);

			 printf("Kp: %.2f\tKi: %.2f\tKd: %.2f",Prop,Int,Der);
	         
		//ALIMENTAZIONE
			 Vout=Prop+Der+Int;								//in questo caso 'Vout' si riferisce al Dewty Cicle
			 printf("\tTm: %.2fC",Tmed);
		    
 		// Invio Tensione all'attuatore

			Vout=Vout*0.1; //riscalo [0-1]

			printf("\tDy: %.2f\n",Vout);

			PWMout(&hTaskHandlePWM, Vout);
			
		//Scrittura su file
        	fileout.open(NomeFile.c_str(), std::fstream::app); // ...app: APPend, concatena a file esistente
        	fileout<< Tmed <<","<<Vout<<endl;
        	fileout.close();
			
			
		//Inizializzo Errore Precedente
			 errMedP=errMed;
			 c++;
	 
		}

	}
		

//Spegnimento strumento
	DAQmxStopTask(lettura);
	DAQmxClearTask(lettura);
	ErrorCheck(DAQmxStopTask(hTaskHandlePWM));
	ErrorCheck(DAQmxClearTask(hTaskHandlePWM));
	
	getchar();
	return 0;
}
	

