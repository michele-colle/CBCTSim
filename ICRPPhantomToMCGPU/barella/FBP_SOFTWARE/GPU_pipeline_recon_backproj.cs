using Cloo;
using Cloo.Bindings;
using Emgu.CV;
using Emgu.CV.Structure;
using System;
using System.Linq;
using System.Runtime.InteropServices;

namespace FBP.Model
{
    public class GPU_pipeline_recon_backproj
    {
        string Reconstruction = @"float GetReconstructedValue(
    __global float *Matrice, __constant float *ScanSin,
    __constant float *ScanCos, __constant float *TiltSin,
    __constant float *TiltCos, __constant float *iTilt,
    __constant float *OffsetU, __constant float *OffsetV, __constant int *sizes,
    __constant float *DSD, __constant float *DSO, __constant float *SizeVoxel,
    __constant float *one_over_SizeVoxel, __constant float *PixelSensoreDim,
    __constant float *one_over_PixelSensoreDim, __constant int *DualEnergy,
    __constant float *weightFactor, __constant float *vol_midpoint_trasl,
    __constant float *r2_min_rec, __constant float *r2_max0,
    __constant float *r2_max1, __constant int *out_of_plane_rot,
    __constant float *p, int3 GlobalId) {
  long _SizerecDual;
  int semiSzX, semiSzY, semiSzZ, semiSzImgX, semiSzImgY, SzX, SzY, SzZ, simgX,
      simgY, SrecPlane;
  float Sen_th, Cos_th, dsd, dso, dso2, sizeVx, one_over_sizeVx,
      one_over_size_Px;
  float x1_rec_mm, y1_rec_mm, x1, y1, z1, backproj_factor, u, v, u1, v1, x_det,
      y_det, weight, x1px, y1px, D;
  float A, B, _ValImage1, _ValImage2, _ValImage3, _ValImage4, _ValImage, rho,
      r2_max_sel;
  int i1, i2, i3, i4, ixdet, iydet, ixdetp1, iydetp1;
  _ValImage = 0;
  SzX = sizes[0];
  SzY = sizes[1];
  SzZ = sizes[2];
  simgX = sizes[3];
  simgY = sizes[4];
  semiSzX = sizes[5];
  semiSzY = sizes[6];
  semiSzZ = sizes[7];
  semiSzImgX = sizes[8];
  semiSzImgY = sizes[9];

  SrecPlane = SzX * SzY;

  dsd = DSD[0];
  dso = DSO[0];
  dso2 = DSO[1];
  sizeVx = SizeVoxel[0];
  one_over_sizeVx = one_over_SizeVoxel[0];
  one_over_size_Px = one_over_PixelSensoreDim[0];

  Sen_th = ScanSin[0];
  Cos_th = ScanCos[0];

  D = dsd - dso;

  _SizerecDual = (SrecPlane * DualEnergy[0] * SzZ);

  int ix = GlobalId.x; // indici della matrice
  int iy = GlobalId.y;
  int iz = GlobalId.z;


  // coordinate del cilindro di ricostruzionie dal centro in millimetri tenendo
  // contok della traslazione del centro
  x1_rec_mm = ((ix)-semiSzX + vol_midpoint_trasl[0]) * sizeVx;
  y1_rec_mm = ((iy)-semiSzY + vol_midpoint_trasl[1]) * sizeVx;
  z1 = ((iz)-semiSzZ + vol_midpoint_trasl[2]) * sizeVx;

  if (z1 < 0)
    r2_max_sel = r2_max0[iz];
  else
    r2_max_sel = r2_max1[iz];

  if (r2_max_sel <= 0)
    return 0;

  // coordinate della matrice ruotata in millimetri, disegno in kak - Principles
  // of computerized tomographic imaging
  // pagina 102 fig. 3.35

  x1 = x1_rec_mm * Cos_th - y1_rec_mm * Sen_th;  // t
  y1 = -x1_rec_mm * Sen_th - y1_rec_mm * Cos_th; // s

  x1px = x1 * one_over_sizeVx;
  y1px = y1 * one_over_sizeVx;
  rho = x1px * x1px + y1px * y1px;
  if (rho > r2_min_rec[0] || rho >= r2_max_sel)
    return 0;

  // coordinate del detector in millimetri e offset
  backproj_factor = dsd / (dso - y1);//dsd impatta sulla ricostruzione mentre dso solo sul fattore di zoom

  u = backproj_factor * x1; // x del detector mm
  v = backproj_factor * z1; // y del detector mm
  // applico detector titlt
  u1 = u;
  v1 = v;
  if (out_of_plane_rot[0] != 0) {
    u = p[0] * u1 + p[1] * D + p[2] * v1 + p[3];
    v = p[4] * u1 + p[5] * D + p[6] * v1 + p[7];
    float w = p[8] * u1 + p[9] * D + p[10] * v1 + p[11];
    x_det =
        (u / w - p[3] - OffsetU[0]) * one_over_size_Px + (semiSzImgX) + 0.5f;
    y_det =
        (v / w - p[7] - OffsetV[0]) * one_over_size_Px + (semiSzImgY) + 0.5f;
  } else {
    u = u - OffsetU[0];
    v = v - OffsetV[0];
    u1 = u * TiltCos[0] - v * TiltSin[0];
    v1 = u * TiltSin[0] + v * TiltCos[0];

    // converto in pixel, e mi sposto sullo spigolo

    x_det = u1 * one_over_size_Px + (semiSzImgX) + 0.5f;
    y_det = v1 * one_over_size_Px + (semiSzImgY) + 0.5f;
  }

  // calcolo dei pesi per la ricostruzione
  // weight = 1;
  weight = dso2 / ((dso - y1) * (dso - y1));
  // interpolazione lineare

  if (x_det >= 0 && x_det < simgX && y_det >= 0 && y_det < simgY) {
    ixdet = (int)floor(x_det);
    iydet = (int)floor(y_det);
    A = x_det - floor(x_det);
    B = y_det - floor(y_det);

    if (ixdet + 1 < simgX)
      ixdetp1 = ixdet + 1;
    else
      ixdetp1 = simgX - 1;

    if (iydet + 1 < simgY)
      iydetp1 = iydet + 1;
    else
      iydetp1 = simgY - 1;

    i1 = iydet * simgX + ixdet;
    i2 = iydet * simgX + ixdetp1;
    i3 = iydetp1 * simgX + ixdet;
    i4 = iydetp1 * simgX + ixdetp1;

    _ValImage1 = Matrice[i1]; // Pixel dell'immagine con coordinate calcolate da
                              // ValX e ValY
    _ValImage2 = Matrice[i2]; // Pixel dell'immagine con coordinate calcolate da
                              // ValX e ValY
    _ValImage3 = Matrice[i3]; // Pixel dell'immagine con coordinate calcolate da
                              // ValX e ValY
    _ValImage4 = Matrice[i4]; // Pixel dell'immagine con coordinate calcolate da
                              // ValX e ValY

    _ValImage = _ValImage4 * A * B + _ValImage3 * (1 - A) * B +
                _ValImage2 * A * (1 - B) + _ValImage1 * (1 - A) * (1 - B);
    _ValImage = _ValImage * weight * weightFactor[0];

    if (isnan(_ValImage) == 1)
      _ValImage = 0.0f;
  }
  return _ValImage;
}

// #pragma OPENCL EXTENSION cl_khr_fp16 : enable
__kernel void ReconstructionGPU(
    __global float *Matrice, __global float *ReturnMatrice,
    __constant float *ScanSin, __constant float *ScanCos,
    __constant float *TiltSin, __constant float *TiltCos,
    __constant float *iTilt, __constant float *OffsetU,
    __constant float *OffsetV, __constant int *sizes, __constant float *DSD,
    __constant float *DSO, __constant float *SizeVoxel,
    __constant float *one_over_SizeVoxel, __constant float *PixelSensoreDim,
    __constant float *one_over_PixelSensoreDim, __constant int *DualEnergy,
    __constant float *weightFactor, __constant float *vol_midpoint_trasl,
    __constant float *r2_min_rec, __constant float *r2_max0,
    __constant float *r2_max1, __constant int *out_of_plane_rot,
    __constant float *p) {

  int SzX = sizes[0];
  int SzY = sizes[1];
  int SzZ = sizes[2];

  int SrecPlane = SzX * SzY;

  long _SizerecDual = (SrecPlane * DualEnergy[0] * SzZ);

  int ix = get_global_id(0); // indici della matrice
  int iy = get_global_id(1);
  int iz = get_global_id(2);

if (ix >= SzX || iy >= SzY || iz >= SzZ)
    return; // fuori dalla matrice

  int3 gloId = (int3)(ix, iy, iz);
  float _ValImage = GetReconstructedValue(
      Matrice, ScanSin, ScanCos, TiltSin, TiltCos, iTilt, OffsetU, OffsetV,
      sizes, DSD, DSO, SizeVoxel, one_over_SizeVoxel, PixelSensoreDim,
      one_over_PixelSensoreDim, DualEnergy, weightFactor, vol_midpoint_trasl,
      r2_min_rec, r2_max0, r2_max1, out_of_plane_rot, p, gloId);

  if (_ValImage != 0)
    ReturnMatrice[_SizerecDual + ix + iy * SzX + iz * SrecPlane] +=
        _ValImage; // ValImgPesato
}



__kernel void ReconstructionGPU_half(
    __global float *Matrice, __global half *ReturnMatrice,
    __constant float *ScanSin, __constant float *ScanCos,
    __constant float *TiltSin, __constant float *TiltCos,
    __constant float *iTilt, __constant float *OffsetU,
    __constant float *OffsetV, __constant int *sizes, __constant float *DSD,
    __constant float *DSO, __constant float *SizeVoxel,
    __constant float *one_over_SizeVoxel, __constant float *PixelSensoreDim,
    __constant float *one_over_PixelSensoreDim, __constant int *DualEnergy,
    __constant float *weightFactor, __constant float *vol_midpoint_trasl,
    __constant float *r2_min_rec, __constant float *r2_max0,
    __constant float *r2_max1, __constant int *out_of_plane_rot,
    __constant float *p) {

  int SzX = sizes[0];
  int SzY = sizes[1];
  int SzZ = sizes[2];

  int SrecPlane = SzX * SzY;

  long _SizerecDual = (SrecPlane * DualEnergy[0] * SzZ);

  int ix = get_global_id(0); // indici della matrice
  int iy = get_global_id(1);
  int iz = get_global_id(2);
  int3 gloId = (int3)(ix, iy, iz);
  float _ValImage = GetReconstructedValue(
      Matrice, ScanSin, ScanCos, TiltSin, TiltCos, iTilt, OffsetU, OffsetV,
      sizes, DSD, DSO, SizeVoxel, one_over_SizeVoxel, PixelSensoreDim,
      one_over_PixelSensoreDim, DualEnergy, weightFactor, vol_midpoint_trasl,
      r2_min_rec, r2_max0, r2_max1, out_of_plane_rot, p, gloId);

  if (_ValImage != 0) {
    float tRead = vload_half(_SizerecDual + ix + iy * SzX + iz * SrecPlane,
                             ReturnMatrice);
    vstore_half(tRead + _ValImage,
                _SizerecDual + ix + iy * SzX + iz * SrecPlane, ReturnMatrice);
    // ReturnMatrice[_SizerecDual + ix + iy * SzX + iz * SrecPlane ] +=
    // _ValImage;   //ValImgPesato
  }
}

// #pragma OPENCL EXTENSION cl_khr_fp16 : enable
__kernel void ReconstructionGPUSingleSlice(
    __global float *Matrice, __global float *ReturnMatrice,
    __constant float *ScanSin, __constant float *ScanCos,
    __constant float *TiltSin, __constant float *TiltCos,
    __constant float *iTilt, __constant float *OffsetU,
    __constant float *OffsetV, __constant int *sizes, __constant float *DSD,
    __constant float *DSO, __constant float *SizeVoxel,
    __constant float *one_over_SizeVoxel, __constant float *PixelSensoreDim,
    __constant float *one_over_PixelSensoreDim, __constant int *DualEnergy,
    __constant float *weightFactor, __constant float *vol_midpoint_trasl,
    __constant float *r2_min_rec, __constant float *r2_max0,
    __constant float *r2_max1, __constant int *out_of_plane_rot,
    __constant float *p,    int nSlice) {

  int SzX = sizes[0];
  int SzY = sizes[1];
  int SzZ = sizes[2];

  int SrecPlane = SzX * SzY;

  long _SizerecDual = (SrecPlane * DualEnergy[0] * SzZ);

  int ix = get_global_id(0); // indici della matrice
  int iy = get_global_id(1);
  int iz = nSlice;
  int3 gloId = (int3)(ix, iy, iz);
  float _ValImage = GetReconstructedValue(
      Matrice, ScanSin, ScanCos, TiltSin, TiltCos, iTilt, OffsetU, OffsetV,
      sizes, DSD, DSO, SizeVoxel, one_over_SizeVoxel, PixelSensoreDim,
      one_over_PixelSensoreDim, DualEnergy, weightFactor, vol_midpoint_trasl,
      r2_min_rec, r2_max0, r2_max1, out_of_plane_rot, p, gloId);

  if (_ValImage != 0)
    ReturnMatrice[ix + iy * SzX] +=
        _ValImage; // ValImgPesato
 //ReturnMatrice[ix + iy * SzX] = 1234;
}

// #pragma OPENCL EXTENSION cl_khr_fp16 : enable
__kernel void ReconstructionGPUSingleSliceHalf(
    __global float *Matrice, __global half *ReturnMatrice,
    __constant float *ScanSin, __constant float *ScanCos,
    __constant float *TiltSin, __constant float *TiltCos,
    __constant float *iTilt, __constant float *OffsetU,
    __constant float *OffsetV, __constant int *sizes, __constant float *DSD,
    __constant float *DSO, __constant float *SizeVoxel,
    __constant float *one_over_SizeVoxel, __constant float *PixelSensoreDim,
    __constant float *one_over_PixelSensoreDim, __constant int *DualEnergy,
    __constant float *weightFactor, __constant float *vol_midpoint_trasl,
    __constant float *r2_min_rec, __constant float *r2_max0,
    __constant float *r2_max1, __constant int *out_of_plane_rot,
    __constant float *p,    int nSlice) {

  int SzX = sizes[0];
  int SzY = sizes[1];
  int SzZ = sizes[2];

  int SrecPlane = SzX * SzY;

  long _SizerecDual = (SrecPlane * DualEnergy[0] * SzZ);

  int ix = get_global_id(0); // indici della matrice
  int iy = get_global_id(1);
  int iz = nSlice;
  int3 gloId = (int3)(ix, iy, iz);
  float _ValImage = GetReconstructedValue(
      Matrice, ScanSin, ScanCos, TiltSin, TiltCos, iTilt, OffsetU, OffsetV,
      sizes, DSD, DSO, SizeVoxel, one_over_SizeVoxel, PixelSensoreDim,
      one_over_PixelSensoreDim, DualEnergy, weightFactor, vol_midpoint_trasl,
      r2_min_rec, r2_max0, r2_max1, out_of_plane_rot, p, gloId);

  if (_ValImage != 0) {
    float tRead = vload_half(ix + iy * SzX, ReturnMatrice);
    vstore_half(tRead + _ValImage,ix + iy * SzX, ReturnMatrice);
  }
}

__kernel void TestSetNumber(
    __global float *Matrice, __constant int *sizes)
{
  int ix = get_global_id(0); // indici della matrice
  int iy = get_global_id(1);
    int SzX = sizes[0];
  int SzY = sizes[1];
  int SzZ = sizes[2];
   Matrice[ix + iy * SzX] = 1234;
}

__kernel void HalfToFloat(__global half *halfIn, __global float *floatOut){
  int i = get_global_id(0);
  float tRead = vload_half(i, halfIn);
  floatOut[i] = tRead;
}
";





        public ComputeKernel kernel { get; private set; }
        public ComputeKernel kernelTest { get; private set; }
        public ComputeKernel kernelHalfToFloat { get; private set; }

        ComputeBuffer<short>[] _Result_half;
        //ComputeBuffer<float> _Result;
        ComputeBuffer<float>[] _Result;
        ComputeBuffer<float> ScanSin;
        ComputeBuffer<float> ScanCos;
        ComputeBuffer<float> TiltSin;
        ComputeBuffer<float> TiltCos;
        ComputeBuffer<float> Tilt;
        ComputeBuffer<float> OffsetU;
        ComputeBuffer<float> OffsetV;
        ComputeBuffer<int> sizes;
        ComputeBuffer<float> DSD;
        ComputeBuffer<float> DSO;
        ComputeBuffer<float> SizeVoxel;
        ComputeBuffer<float> one_over_sizeVoxel;
        ComputeBuffer<float> PixelSensoreDim;
        ComputeBuffer<float> one_over_PixelSensoreDim;
        ComputeBuffer<int> DualEnergy;
        ComputeBuffer<float> weightFactor;
        ComputeBuffer<float> vol_midp_trasl;
        ComputeBuffer<float> r2_min_rec;
        ComputeBuffer<float> r2_max0;
        ComputeBuffer<float> r2_max1;
        ComputeBuffer<float> r2;
        ComputeBuffer<float> recon_weight;
        ComputeBuffer<int> out_of_plane_rot;
        ComputeBuffer<float> p;
        int recw, rech, recl;
        bool DE, half;
        ComputeProgram program;
        private static readonly log4net.ILog logger = log4net.LogManager.GetLogger(System.Reflection.MethodBase.GetCurrentMethod().DeclaringType);
        public GPU_pipeline_recon_backproj(
            ComputeContext context,
            ComputeBuffer<float> matrixIn,
            int recsizex,
            int recsizey,
            int recsizez,
            bool dualEnergy,
            int sizeImxgX,
            int sizeImxgY,
            float VoxelDim,
            float pixelDim,
            float[] vol_midp_trasl,
            float r2_min_rec,
            float[] r2_max0,
            float[] r2_max1
            )
        {
            long max_mem = context.Devices[0].GlobalMemorySize;
            //if (context.Platform.Name.Contains("NVIDIA"))
            //    max_mem = max_mem / 4;

            long max_mem_Mbytes = max_mem / 1000000;
            recw = recsizex;
            rech = recsizey;
            recl = recsizez;
            DE = dualEnergy;
            int mul = DE ? 2 : 1;
            long totlen = (long)recsizex * (long)recsizey * (long)recsizez * (long)mul;
            long totlen_bytes = (long)recsizex * (long)recsizey * (long)recsizez * (long)mul * 4;
            long totlen_Mbytes = totlen_bytes / 1000000;

            half = false;

            if (totlen_bytes >= max_mem)
            {
                half = true;
                totlen_bytes = (long)recsizex * (long)recsizey * (long)recsizez * (long)mul * 2;
                //if (totlen_bytes >= max_mem)
                //  throw new Exception("stack dimension exceedes graphic card MaxMemoryAllocationSize: " + max_mem_Mbytes.ToString() + " Mb");
            }


            //half = true;



            ////inizializzo le variabili in gpu
            //if (half)
            //    _Result_half = new ComputeBuffer<short>(context, ComputeMemoryFlags.ReadWrite, totlen);
            //else
            //    _Result = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite ,totlen);
            if (!half)
            {

                _Result = new ComputeBuffer<float>[(long)recsizez * (long)mul];
                for (int i = 0; i < (long)recsizez * (long)mul; i++)
                {
                    _Result[i] = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite, (long)recsizex * (long)recsizey);
                }
            }
            else
            {
                _Result_half = new ComputeBuffer<short>[(long)recsizez * (long)mul];
                for (int i = 0; i < (long)recsizez * (long)mul; i++)
                {
                    _Result_half[i] = new ComputeBuffer<short>(context, ComputeMemoryFlags.ReadWrite, (long)recsizex * (long)recsizey);
                }
            }

            ScanSin = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[1] { 0 });
            ScanCos = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[1] { 0 });
            TiltCos = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[1] { 0 });
            TiltSin = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[1] { 0 });
            Tilt = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[1] { 0 });
            OffsetU = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[1] { 0 });
            OffsetV = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[1] { 0 });
            sizes = new ComputeBuffer<int>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new int[] { recsizex, recsizey, recsizez, sizeImxgX, sizeImxgY, recsizex / 2, recsizey / 2, recsizez / 2, sizeImxgX / 2, sizeImxgY / 2 });
            DSD = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[1] { 0 });
            DSO = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[] { 0, 0 });
            SizeVoxel = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[1] { VoxelDim });
            one_over_sizeVoxel = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[1] { (float)(1.0 / VoxelDim) });
            PixelSensoreDim = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[1] { pixelDim });
            one_over_PixelSensoreDim = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[1] { (float)(1.0 / pixelDim) });
            DualEnergy = new ComputeBuffer<int>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new int[1] { 0 });
            weightFactor = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[1] { 0 });
            this.vol_midp_trasl = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, vol_midp_trasl);
            this.r2_min_rec = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[] { r2_min_rec });
            this.r2_max0 = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, r2_max0);
            this.r2_max1 = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, r2_max1);
            out_of_plane_rot = new ComputeBuffer<int>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new int[1] { 0 });
            p = new ComputeBuffer<float>(context, ComputeMemoryFlags.ReadWrite | ComputeMemoryFlags.CopyHostPointer, new float[12]);

            //creo il kernel
            program = new ComputeProgram(context, Reconstruction);
            try
            {
                program.Build(null, null, null, IntPtr.Zero);
            }
            catch (Exception ex)
            {
                CLDeviceHandle[] device_list = program.Devices.Select(d => d.Handle).ToArray();
                ComputeErrorCode errorCode = CL10.BuildProgram(program.Handle, device_list.Length, device_list, "", null, IntPtr.Zero);
                CL10.GetProgramBuildInfo(program.Handle, context.Devices[0].Handle, ComputeProgramBuildInfo.BuildLog, IntPtr.Zero, IntPtr.Zero, out IntPtr param_value_size_ret);
                byte[] log = new byte[param_value_size_ret.ToInt32()];
                GCHandle pinnedArray = GCHandle.Alloc(log, GCHandleType.Pinned);
                IntPtr pointer = pinnedArray.AddrOfPinnedObject();
                CL10.GetProgramBuildInfo(program.Handle, context.Devices[0].Handle, ComputeProgramBuildInfo.BuildLog, param_value_size_ret, pointer, out param_value_size_ret);
                pinnedArray.Free();
                var str = System.Text.Encoding.ASCII.GetString(log);
                Console.Write(str);
                logger.Fatal(str);
                throw ex;
            }


            if (half)
                kernel = program.CreateKernel("ReconstructionGPUSingleSliceHalf");
            else
                kernel = program.CreateKernel("ReconstructionGPUSingleSlice");

            kernelHalfToFloat = program.CreateKernel("HalfToFloat");



            //setto gli argomenti del kernel
            kernel.SetMemoryArgument(0, matrixIn);//
            //if(half)
            //    kernel.SetMemoryArgument(1, _Result_half);//
            //else
            //    kernel.SetMemoryArgument(1, _Result);//
            kernel.SetMemoryArgument(2, ScanSin);//
            kernel.SetMemoryArgument(3, ScanCos);//
            kernel.SetMemoryArgument(4, TiltSin);//
            kernel.SetMemoryArgument(5, TiltCos);//
            kernel.SetMemoryArgument(6, Tilt);//
            kernel.SetMemoryArgument(7, OffsetU);//
            kernel.SetMemoryArgument(8, OffsetV);//
            kernel.SetMemoryArgument(9, sizes);//
            kernel.SetMemoryArgument(10, DSD);//
            kernel.SetMemoryArgument(11, DSO);//
            kernel.SetMemoryArgument(12, SizeVoxel);
            kernel.SetMemoryArgument(13, one_over_sizeVoxel);
            kernel.SetMemoryArgument(14, PixelSensoreDim);
            kernel.SetMemoryArgument(15, one_over_PixelSensoreDim);//
            kernel.SetMemoryArgument(16, DualEnergy);//
            kernel.SetMemoryArgument(17, weightFactor);//
            kernel.SetMemoryArgument(18, this.vol_midp_trasl);//
            kernel.SetMemoryArgument(19, this.r2_min_rec);//
            kernel.SetMemoryArgument(20, this.r2_max0);//
            kernel.SetMemoryArgument(21, this.r2_max1);//
            kernel.SetMemoryArgument(22, out_of_plane_rot);//
            kernel.SetMemoryArgument(23, p);//
                                            //kernel.SetMemoryArgument(26, this.r2);//
                                            //kernel.SetMemoryArgument(27, this.recon_weight);//


        }

        public void write_params_and_execute(ComputeCommandQueue commands, long[] dims, ReconstructionParameters REcParLoc, float cos_th, float sen_th, int energia, float deltaTheta)
        {
            //REcParLoc.psi = 1;
            if (REcParLoc.projection_matrix != null)
            {
                commands.WriteToBuffer(new float[] { 0 }, Tilt, true, null);
                commands.WriteToBuffer(new float[] { 0 }, TiltSin, true, null);
                commands.WriteToBuffer(new float[] { 1 }, TiltCos, true, null);
                commands.WriteToBuffer(new int[] { 1 }, out_of_plane_rot, true, null);
                commands.WriteToBuffer(REcParLoc.projection_matrix, p, true, null);
            }
            else
            {
                commands.WriteToBuffer(new int[] { 0 }, out_of_plane_rot, true, null);
                commands.WriteToBuffer(new float[] { (float)REcParLoc.tilt }, Tilt, true, null);
                commands.WriteToBuffer(new float[] { (float)Math.Sin(REcParLoc.tilt_rad) }, TiltSin, true, null);
                commands.WriteToBuffer(new float[] { (float)Math.Cos(REcParLoc.tilt_rad) }, TiltCos, true, null);
            }
            commands.WriteToBuffer(new int[] { energia }, DualEnergy, true, null);
            commands.WriteToBuffer(new float[] { REcParLoc.offX_mm }, OffsetU, true, null);
            commands.WriteToBuffer(new float[] { REcParLoc.GetOffY() }, OffsetV, true, null);
            commands.WriteToBuffer(new float[] { (float)REcParLoc.dsd }, DSD, true, null);
            commands.WriteToBuffer(new float[] { (float)REcParLoc.dso, (float)(REcParLoc.dso * REcParLoc.dso) }, DSO, true, null);


            commands.WriteToBuffer(new float[] { cos_th }, ScanCos, true, null);
            commands.WriteToBuffer(new float[] { sen_th }, ScanSin, true, null);

            //commands.WriteToBuffer(recon_weight, this.recon_weight, true, null);
            //commands.Finish();

            float factor = (float)(deltaTheta * 0.5 / (REcParLoc.px_det * REcParLoc.dso / REcParLoc.dsd));
            if (REcParLoc.parker > 0)
                factor *= 2;

            commands.WriteToBuffer(new float[] { factor }, weightFactor, true, null);

            long[] dims0 = new long[] { (dims[0]/4+1)*4, (dims[1]/4+1)*4, 1 };
            for (int i = 0; i < dims[2]; i++)
            {
                kernel.SetValueArgument(24, i);
                if (!half)
                {
                    var buf = _Result[i + energia * dims[2]];
                    kernel.SetMemoryArgument(1, buf);
                }
                else
                {
                    var buf = _Result_half[i + energia * dims[2]];
                    kernel.SetMemoryArgument(1, buf);
                }
                commands.Execute(kernel, null, dims0, null, null);
                //

            }
            commands.Finish();

            //test_recon_cpu(0, 0, sen_th, cos_th, (float)Math.Cos(REcParLoc[4] * Math.PI / 180.0), (float)Math.Sin(REcParLoc[4] * Math.PI / 180.0), REcParLoc[4],
            //  REcParLoc[2], REcParLoc[3],512,512,512,512,512, REcParLoc[0], REcParLoc[1],REcParLoc[12],REcParLoc[12],REcParLoc[11],0,1);

        }


        public Image<Gray, float>[] read_reconstructed_matrix(ComputeCommandQueue commands)
        {
            int mul = DE ? 2 : 1;
            int stoutl = recl * mul;
            var stout = new Image<Gray, float>[stoutl];
            long arr_len = recw * rech; //questo DEVE essere un long altrimenti la lettura da scheda video per volumi grandi non funziona
            short[] tmpArrh = new short[arr_len];
            float[] tmpArr = new float[arr_len];
            byte[] tmpArrByte = new byte[arr_len * 4];
            for (int i = 0; i < stoutl; i++)
            {
                stout[i] = new Image<Gray, float>(recw, rech);
            }
            ComputeBuffer<float> ToRead = new ComputeBuffer<float>(commands.Context, ComputeMemoryFlags.ReadWrite, (long)recw * (long)rech);
            ComputeBuffer<short> toConvert;
            for (int i = 0; i < stoutl; i++)
            {
                if (half)
                {
                    toConvert = _Result_half[i];
                    kernelHalfToFloat.SetMemoryArgument(0, toConvert);
                    kernelHalfToFloat.SetMemoryArgument(1, ToRead);
                    commands.Execute(kernelHalfToFloat, null, new long[] { recw * rech, 1, 1 }, null, null);
                    commands.Finish();
                }
                else
                    ToRead = _Result[i];

                commands.ReadFromBuffer(ToRead, ref tmpArr, true, 0, 0, arr_len, null);
                commands.Finish();



                Buffer.BlockCopy(tmpArr, 0, tmpArrByte, 0, (int)(arr_len * 4));
                stout[i].Bytes = tmpArrByte;

                _Result?[i]?.Dispose();
                _Result_half?[i]?.Dispose();
            }

            return stout;

        }
        public Image<Gray, float>[] read_reconstructed_matrix2(ComputeCommandQueue commands)
        {
            int mul = DE ? 2 : 1;
            int stoutl = recl * mul;
            var stout = new Image<Gray, float>[stoutl];
            int arr_len = recw * rech;
            short[] tmpArr = new short[arr_len];
            for (int i = 0; i < stoutl; i++)
            {
                stout[i] = new Image<Gray, float>(recw, rech);
                //commands.ReadFromBuffer(_Result,ref tmpArr, true, i* arr_len, 0, arr_len, null);
                commands.Finish();
                for (int y = 0; y < rech; y++)
                {
                    for (int x = 0; x < recw; x++)
                    {
                        int j = x + y * recw;
                        stout[i].Data[y, x, 0] = toFloat(tmpArr[j]);
                        if (stout[i].Data[y, x, 0] != 0)
                        {

                        }
                    }
                }
            }

            return stout;

        }



        public static float toFloat(int hbits)
        {
            int mant = hbits & 0x03ff;            // 10 bits mantissa
            int exp = hbits & 0x7c00;            // 5 bits exponent
            if (exp == 0x7c00)                   // NaN/Inf
                exp = 0x3fc00;                    // -> NaN/Inf
            else if (exp != 0)                   // valore normalizzato
            {
                exp += 0x1c000;                   // exp - 15 + 127
                if (mant == 0 && exp > 0x1c400)  // 
                    return BitConverter.ToSingle(BitConverter.GetBytes((hbits & 0x8000) << 16
                                                    | exp << 13 | 0x3ff), 0);
            }
            else if (mant != 0)                  // && exp==0 -> subnormal
            {
                exp = 0x1c400;                    // normalizza
                do
                {
                    mant <<= 1;                   // mantissa * 2
                    exp -= 0x400;                 // 
                } while ((mant & 0x400) == 0); // qundo non è vero
                mant &= 0x3ff;                    // filtro bit estranei
            }                                     // else +/-0 -> +/-0
            return BitConverter.ToSingle(BitConverter.GetBytes(          // combina il valore
                (hbits & 0x8000) << 16          // sign  << ( 31 - 15 )
                | (exp | mant) << 13), 0);         // value << ( 23 - 10 )
        }

        public void dispose()
        {
            kernel.Dispose();
            //if (_Result != null)
            //{
            //    for (int i = 0; i < _Result.Length; i++)
            //    {
            //        _Result[i].Dispose();
            //    }
            //}
            //if (_Result_half != null)
            //{
            //    for (int i = 0; i < _Result_half.Length; i++)
            //    {
            //        _Result_half[i].Dispose();
            //    }
            //}

            ScanSin.Dispose();
            ScanCos.Dispose();
            TiltCos.Dispose();
            TiltSin.Dispose();
            Tilt.Dispose();
            OffsetU.Dispose();
            OffsetV.Dispose();
            sizes.Dispose();
            DSD.Dispose();
            DSO.Dispose();
            SizeVoxel.Dispose();
            one_over_sizeVoxel.Dispose();
            PixelSensoreDim.Dispose();
            one_over_PixelSensoreDim.Dispose();
            DualEnergy.Dispose();
            weightFactor.Dispose();
            vol_midp_trasl.Dispose();
            r2_min_rec.Dispose();
            r2_max0.Dispose();
            r2_max1.Dispose();
            r2?.Dispose();
            recon_weight?.Dispose();
        }

        //private void notify(CLProgramHandle programHandle, IntPtr userDataPtr)
        //{
        //    Console.WriteLine("Program build notification.");
        //    byte[] bytes = program.Binaries[0];
        //    Console.WriteLine("Beginning of program binary (compiled for the 1st selected device):");
        //    Console.WriteLine(BitConverter.ToString(bytes, 0, 24) + "...");
        //}

    }
}
