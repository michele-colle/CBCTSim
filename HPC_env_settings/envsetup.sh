spack env create adept "/home/colle/Desktop/CBCTSim/CBCTSimCeleritasTest/CBCTSim/celeritas docker spack/adept-env.yaml"
spack env activate adept
#va modificato il file yaml per inserire la corretta architettura cuda, da cercare nelle specifiche della scheda
#in toeria si puo fare con questo comando ma ho dei dubbi:
#spack config add "packages:all:variants:cxxstd=20 +cuda cuda_arch=75"

spack install -j $(nproc)


#dopo aver clonato il repos di 
git clone https://github.com/mnovak42/g4hepem.git
cd g4hepem
mkdir build && cd build
ccmake ..
#da valutare attentissimamante il discorso sudo!!
sudo cmake --build . --target install -- -j 32

cd ../../AdePT-0.1.7-alpha/build/
