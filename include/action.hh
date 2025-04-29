#ifndef ACTION_HH
#define ACTION_HH

#include <G4VUserActionInitialization.hh>
#include <generator.hh>
#include <run.hh>

class MyActionInitialization : public G4VUserActionInitialization
{
    public:
    MyActionInitialization();
    ~MyActionInitialization();
    
    //worker thread
    virtual void Build() const;
    //master thread
    virtual void BuildForMaster() const;
};
#endif