using DicomLib;
using Emgu.CV;
using Emgu.CV.Structure;
using MathNet.Numerics;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Numerics;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using static FBP.Model.ReconstructionParameters;


namespace FBP.Model
{
    public class Cefla_de_data
    {
        /*
         * distretti attualmente selezinabili al 6 febb 2023
            "Head"
            "Neck"
            "Shoulder"
            "Arm"
            "Chest"
            "Spine"
            "Hip"
            "Leg"
            "Mandibular"
            "TemporalBone"
            "Hand"
            "Elbow"
            "Ankle"
            "Knee"
         */
        public string accession_num;
        public string acquisition_date;
        public string acquisition_time;
        public string address;
        public string anatomic_region;
        public string animal_species;
        public string birth_date;
        public double ctdi;
        public double ctdi_vol;
        public double dap;
        public string device_serial;
        public double dlp;
        public string doccode_exam;
        public string doccode_parent;
        public string document_note;
        public string exam_type;
        public double exp_time;
        public string first_name;
        public string fov;
        public string institution_name;
        public string manufacturer;
        public string manufacturer_model_name;
        public double mas;
        public string patient_id;
        public string patient_position;
        public string ref_physician_name;
        public string sender_doctor;
        public string prot_name;
        public string sex;
        public string station_name;
        public string surname;
        public string sw_ver;
        public float tube_current;
        public double MaxScatterFraction;//verificare se va qua o 

        public cefla_scan_data[] scans;

        [JsonIgnore]
        public string json_path;
        public FOV_pxdim Fov;
        public bool VET = false;


        private static readonly log4net.ILog log = log4net.LogManager.GetLogger(System.Reflection.MethodBase.GetCurrentMethod().DeclaringType);
        public void write_patient_data(out PatientData _sArrPatientData, out string PathPatient, ReconstructionParameters recParams, string NNTPatientDirectory, int nEnergy)
        {
            CultureInfo ci = new CultureInfo("en-US");
            ci.NumberFormat.NumberDecimalSeparator = ".";
            _sArrPatientData = new PatientData();
            _sArrPatientData.first_name = first_name;
            _sArrPatientData.surname = surname;
            _sArrPatientData.patientBirthDate = DateTime.TryParseExact(birth_date, "yyyyMMdd", CultureInfo.InvariantCulture, DateTimeStyles.None, out var res) ? res : new DateTime(1900, 1, 1);
            _sArrPatientData.patientID = patient_id;
            _sArrPatientData.acquisitionDateTime = DateTime.TryParseExact(acquisition_date + acquisition_time, "yyyyMMddHHmmss", CultureInfo.InvariantCulture, DateTimeStyles.None, out var res1) ? res1 : new DateTime(1900, 1, 1, 0, 0, 0);
            _sArrPatientData.kV = (int)scans[nEnergy].kv.Round(1);
            _sArrPatientData.mAs = (double)mas.Round(2);
            _sArrPatientData.protocol = prot_name;
            _sArrPatientData.FOV = fov;
            _sArrPatientData.CTDIVol = (double)ctdi_vol.Round(2);
            _sArrPatientData.XRayTubeCurrent_mA = tube_current;
            _sArrPatientData.DLP_mGy = (double)dlp.Round(2);
            _sArrPatientData.DAP = (double)dap.Round(2);
            _sArrPatientData.sex = sex;



            List<string> lstname = new List<string>();
            var tmpName = first_name != null ? first_name : "";
            lstname.Add(tmpName);
            tmpName = surname != null ? surname : "";
            lstname.Add(tmpName);
            lstname.Add(_sArrPatientData.patientBirthDate.ToString("dd-MM-yyyy"));
            lstname.Add(_sArrPatientData.acquisitionDateTime.ToString("dd-MM-yyyy-HHmm"));
            if (recParams.dual_energy) lstname.Add("DualEnergy");
            else lstname.Add("_" + scans[nEnergy].kv + "kV");
            for (int i = 0; i < lstname.Count; i++)
            {
                lstname[i] = Regex.Replace(lstname[i], @"[^\w\.@-]", "", RegexOptions.None);
            }
            PathPatient = Path.Combine(NNTPatientDirectory, lstname[0] + lstname[1] + "_" + lstname[2] + "_" + lstname[3] + "_" + lstname[4]);
        }
        public void write_patient_dataOLD(ref string[] _sArrPatientData, out string PathPatient, ReconstructionParameters recParams, string NNTPatientDirectory, int nEnergy)
        {
            CultureInfo ci = new CultureInfo("en-US");
            ci.NumberFormat.NumberDecimalSeparator = ".";
            _sArrPatientData[0] = first_name;
            _sArrPatientData[1] = surname;
            _sArrPatientData[2] = birth_date.Substring(6) + "-" +
                birth_date.Substring(4, 2) + "-" +
                birth_date.Substring(0, 4);
            _sArrPatientData[3] = patient_id;
            _sArrPatientData[4] = "";
            _sArrPatientData[5] = "";
            _sArrPatientData[6] = "";
            _sArrPatientData[7] = acquisition_time;
            _sArrPatientData[8] = "";
            _sArrPatientData[9] = (recParams.voxel_side).Round(1).ToString(ci);
            _sArrPatientData[10] = recParams.recX.ToString(ci);
            _sArrPatientData[11] = recParams.recY.ToString(ci);
            _sArrPatientData[12] = "";
            _sArrPatientData[13] = "";
            _sArrPatientData[14] = scans[nEnergy].kv.Round(1).ToString(ci);
            _sArrPatientData[15] = mas.Round(1).ToString(ci);
            _sArrPatientData[16] = prot_name;
            _sArrPatientData[17] = fov;
            _sArrPatientData[18] = ctdi_vol.Round(1).ToString(ci);
            _sArrPatientData[19] = tube_current.ToString(ci);
            _sArrPatientData[20] = "";
            _sArrPatientData[21] = dlp.Round(1).ToString(ci);
            _sArrPatientData[22] = "";
            _sArrPatientData[23] = dap.Round(1).ToString(ci);

            for (int i = 0; i < _sArrPatientData.Length; i++)
            {
                if (_sArrPatientData[i] == null)
                    _sArrPatientData[i] = "";
            }

            List<string> lstname = new List<string>();

            foreach (var i in new int[] { 0, 1, 2, 8, 7 })
            {
                var tmpstr = _sArrPatientData[i].Replace(':', '-');
                tmpstr = tmpstr.Replace(';', '-');
                tmpstr = tmpstr.Replace('\\', '-');
                tmpstr = tmpstr.Replace('/', '-');
                tmpstr = tmpstr.Replace('?', '-');
                tmpstr = tmpstr.Replace('>', '-');
                tmpstr = tmpstr.Replace('<', '-');
                tmpstr = tmpstr.Replace('*', '-');
                tmpstr = tmpstr.Replace(',', '_');
                lstname.Add(tmpstr);
            }
            if (recParams.dual_energy) lstname.Add("_DualEnergy");
            else lstname.Add("_" + scans[nEnergy].kv + "kV");
            PathPatient = Path.Combine(NNTPatientDirectory, lstname[0] + lstname[1] + "_" + lstname[2] + "_" + lstname[3] + "_" + lstname[4] + lstname[5]);
        }
        public void update_dicom_info(ref DicomLib.DicomData dc)
        {
            DicomData.stat_AccessionNumber = accession_num;

            DicomLib.DicomData.stat_BodyPartExamined = anatomic_region;

            DicomLib.DicomData.stat_DeviceSerialNumber = device_serial;
            DicomLib.DicomData.stat_StudyDescription = exam_type;
            DicomLib.DicomData.stat_Manufacturer = manufacturer;
            DicomLib.DicomData.stat_ManufacturersModelName = manufacturer_model_name;
            DicomLib.DicomData.stat_ProtocolName = prot_name;
            DicomLib.DicomData.stat_PatientsSex = sex;
            DicomLib.DicomData.stat_StationName = station_name;
            DicomLib.DicomData.stat_PatientPosition = patient_position;

            DicomLib.DicomData.stat_ImplementationVersionName = "CEFLA_01";
            DicomLib.DicomData.stat_Modality = "CT";
            DicomLib.DicomData.stat_AcquisitionDate = acquisition_date;
            DicomLib.DicomData.stat_AcquisitionTime = acquisition_time;
            DicomLib.DicomData.stat_PatientID = patient_id;

            DicomLib.DicomData.stat_CTDIvol = ctdi_vol.Round(1);
            DicomLib.DicomData.stat_DLP_NotificationTrigger = dlp.Round(1);
            DicomLib.DicomData.stat_DAP = (dap / 100.0).Round(1);

            DicomLib.DicomData.stat_CEFLADetectorDeltaX = scans[0].det_column_pitch.Round(1);
            DicomLib.DicomData.stat_CEFLADetectorDeltaY = scans[0].det_row_pitch.Round(1);
            DicomLib.DicomData.stat_CEFLADoseScansione_mGy = ((scans[0].air_kerma.Sum() + scans[1].air_kerma.Sum()) / 1000.0).Round(1);
            DicomLib.DicomData.stat_CEFLA_doccode_exam = doccode_exam;
            DicomLib.DicomData.stat_CEFLA_doccode_parent = doccode_parent;


            DicomLib.DicomData.stat_ExposureTime = (int)exp_time;
            DicomLib.DicomData.stat_StudyID = short.Parse(DicomLib.DicomWriteClass.GenerateRandomNumber(4));
            DicomData.stat_ReferringPhysiciansName = ref_physician_name;
            DicomData.stat_Address = address;
            DicomData.stat_SeriesDescription = anatomic_region + (document_note != null ? "__" + document_note : null);

        }
        /// <summary>
        /// 
        /// </summary>
        /// <param name="recParam"></param>
        /// <param name="EnergyIndex">0 alta energia, 1 bassa energia</param>
        /// <param name="imgIndex"></param>
        public void write_to_recParameters(ref ReconstructionParameters recParam, int EnergyIndex, int imgIndex)
        {
            recParam.dsd = Math.Round(scans[EnergyIndex].sid[imgIndex], 5);
            recParam.dso = Math.Round(scans[EnergyIndex].sod[imgIndex], 5);

            recParam.scan_angle = (float)Math.Abs(scans[0].angle_eff[scans[0].angle_eff.Length - 1] - scans[0].angle_eff[0]); // ATTENZIONE uso sempre l'angolo dell'alta energia, la bassa viene letta al contrario
            recParam.angle = (float)scans[0].angle_eff[imgIndex]; // ATTENZIONE uso sempre l'angolo dell'alta energia, la bassa viene letta al contrario

            //cerco se la regione anatomica é una di quelle presenti nel dizionario rar e uso la key 
            if (recParam.anatomic_region >= 0)
                if (ReadDualEnergyPar.anatomic_district_rar.ContainsValue(anatomic_region))
                {
                    var myKey = ReadDualEnergyPar.anatomic_district_rar.FirstOrDefault(x => x.Value == anatomic_region).Key;
                    recParam.anatomic_region = (AnatomicRegion)myKey;
                }
                else
                {
                    //altrimenti uso la correzione CEFLA
                    recParam.anatomic_region = AnatomicRegion.cefla;
                }

            //se ho un caso veterinario uso sempre la correzione CEFLA
            if (VET)
                recParam.anatomic_region = AnatomicRegion.cefla;

            recParam.px_det_orig = scans[EnergyIndex].det_column_pitch; //dimensione pixel proporzionato con il resize
            recParam.voxel_side = scans[EnergyIndex].vol_voxel_size[0];//dimensione voxel 


            recParam.sizeX_orig = scans[EnergyIndex].det_columns;
            recParam.sizeY_orig = scans[EnergyIndex].det_rows;
            recParam.recX = (int)scans[EnergyIndex].vol_size[0];
            recParam.recY = (int)scans[EnergyIndex].vol_size[1];
            recParam.recZ = (int)scans[EnergyIndex].vol_size[2];
            //recParam[5] = (float)scans[EnergyIndex].I0[imgIndex]; // verrá messo quando ci saranno i valori di I0 misurati
            recParam.vol_midpoint_translation = new float[] {
                (float)scans[EnergyIndex].vol_midpoint_translation[0],
                (float)scans[EnergyIndex].vol_midpoint_translation[1],
                (float)scans[EnergyIndex].vol_midpoint_translation[2]  };

            recParam.n_proj = (int)scans[0].det_num_proj;
            //recParam[15] ALLVIEWS: se zero uso il parker, da disattivare per scansioni sbandierate, se l'angolo totale é minore di 360 attivo il parker
            var absAngDiff = Math.Abs(scans[0].angle_eff[scans[0].angle_eff.Length - 1] - scans[0].angle_eff[0]);
            if (absAngDiff < 359)
                recParam.parker = 1;//uso il parker
            else
                recParam.parker = 0;//non lo uso


            if (scans[0].angle_eff[1] - scans[0].angle_eff[0] > 0) recParam.CW = false; else recParam.CW = true; //direzione di rotazione, credo vada ignorato conoscendo gli angoli
            //recParam[17] Padding: rimane quello impostato
            // 18 e 19 20 al momento inutilizzate
            //21 median: rimane quello impostato
            //22 filter: rimane quello impostato
            //recParam[24]: nonho capito cosa sia
            //if(scans.Length>1) 
            //    recParam[25] = 1; 
            //else 
            //    recParam[25] = 0;//se ho piu di una scansione é dual energy se no single al momento lo indichiamo nel file di configurazione
            //26 27 non so cosa sia
            //28 dovrebbe essere possibile non utilizzarlo
            //29 QUESTO DEVE ARRIVARE CON IL JSON, SENTIRE SIMONE CEFLA
            recParam.third_parties = true;
            //recParam[16] = 0;non utilizzato se cefla


            //offset: converto in millimetri e poi in pixel resized
            //u: x -> colonne
            //v: y -> righe

            //float avgslant =(float) scans[EnergyIndex].slant.Average();
            //float avgskew = (float)scans[EnergyIndex].skew.Average();
            //float avgtilt = (float)scans[EnergyIndex].tilt.Average();
            //float maxtilt = (float)scans[EnergyIndex].tilt.Max();
            //float mintilt = (float)scans[EnergyIndex].tilt.Min();
            recParam.offX_orig_px = (float)(scans[EnergyIndex].det_columns / 2.0 - scans[EnergyIndex].iso_u[imgIndex] + .5f);//questo é certo 
            recParam.offY_orig_px = (float)(scans[EnergyIndex].det_rows / 2.0 - scans[EnergyIndex].iso_v[imgIndex] + .5f);//questo parrebbe ininfluente
            recParam.psi = (float)(scans[EnergyIndex].slant[imgIndex]);
            recParam.theta = (float)(scans[EnergyIndex].skew[imgIndex]);
            recParam.tilt = Math.Round(scans[EnergyIndex].tilt[imgIndex], 5);
            if (scans[EnergyIndex].matrix_proj != null)
                recParam.projection_matrix = calc_proj_matrix(EnergyIndex, imgIndex);
            //recParam.tilt = 3*(float)avgtilt;
            //recParam.psi = 0.0000000f;//asse X
            //recParam.theta = 0;//asse Y'
            //recParam.tilt = 0;//asse z''

            //recParam.tilt = -0.3f;
            if (imgIndex < 1)
            {
                double avgDsd = Math.Round(scans[EnergyIndex].sid.Average(), 5);
                double avgDso = Math.Round(scans[EnergyIndex].sod.Average(), 5);
                //double tdsd = scans[0].sid[0];
                //double tdso = scans[0].sod[0];
                recParam.calc_parameters(avgDsd, avgDso);
                if (!Enum.TryParse(patient_position, out recParam.patientPosition)) { recParam.patientPosition = PatientPosition.HFS; }
                //switch (recParam.patientPosition)
                //{
                //    case PatientPosition.HFP:
                //        recParam.patientPosition=PatientPosition.FFP;
                //        break;
                //    case PatientPosition.HFS:
                //        recParam.patientPosition = PatientPosition.FFS;
                //        break;
                //    case PatientPosition.HFDR:
                //        recParam.patientPosition = PatientPosition.FFDR;

                //        break;
                //    case PatientPosition.HFDL:
                //        recParam.patientPosition = PatientPosition.FFDL;

                //        break;
                //    case PatientPosition.FFDR:
                //        recParam.patientPosition = PatientPosition.HFDR;

                //        break;
                //    case PatientPosition.FFDL:
                //        recParam.patientPosition = PatientPosition.HFDL;

                //        break;
                //    case PatientPosition.FFP:
                //        recParam.patientPosition = PatientPosition.HFP;

                //        break;
                //    case PatientPosition.FFS:
                //        recParam.patientPosition = PatientPosition.HFS;

                //        break;
                //    case PatientPosition.LFP:
                //        recParam.patientPosition = PatientPosition.LFP;

                //        break;
                //    case PatientPosition.LFS:
                //        recParam.patientPosition = PatientPosition.LFS;

                //        break;
                //    case PatientPosition.RFP:
                //        recParam.patientPosition = PatientPosition.RFP;

                //        break;
                //    case PatientPosition.RFS:
                //        recParam.patientPosition = PatientPosition.RFS;

                //        break;
                //    case PatientPosition.AFDR:
                //        recParam.patientPosition = PatientPosition.AFDR;

                //        break;
                //    case PatientPosition.AFDL:
                //        recParam.patientPosition = PatientPosition.AFDL;

                //        break;
                //    case PatientPosition.PFDR:
                //        recParam.patientPosition = PatientPosition.PFDR;

                //        break;
                //    case PatientPosition.PFDL:
                //        recParam.patientPosition = PatientPosition.PFDL;

                //        break;
                //    default:
                //        break;
                //}
            }

            //se il fov é il 15*06 disattivo l'interpolazione
            if (Fov == FOV_pxdim.fov15x06_154um)
            {
                recParam.interpolation = 0;
            }



            recParam.bCefla = true;
        }

        float[] calc_proj_matrix(int EnergyIndex, int imgIndex)
        {
            float[] arr = scans[EnergyIndex].matrix_proj[imgIndex].Select(x => (float)x).ToArray();
            Matrix4x4 M = new Matrix4x4(arr[0], arr[1], arr[2], arr[3], arr[4], arr[5], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11], 0, 0, 0, 1);
            float s = (float)Math.Sin(-scans[EnergyIndex].angle_eff[imgIndex] * Math.PI / 180.0);
            float c = (float)Math.Cos(-scans[EnergyIndex].angle_eff[imgIndex] * Math.PI / 180.0);
            Matrix4x4 rot = new Matrix4x4(
                c, s, 0, 0,
                -s, c, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1
                );
            var m1 = Matrix4x4.Multiply(M, rot);
            float d = (float)(scans[EnergyIndex].sid[imgIndex] - scans[EnergyIndex].sod[imgIndex]);
            Vector3 pdet = new Vector3(50, d, 50);
            var pdet1 = Vector3.Transform(pdet, Matrix4x4.Transpose(m1));
            float tx = (float)((pdet1.X / pdet1.Z - arr[3]) * scans[EnergyIndex].det_column_pitch);
            float ty = (float)((pdet1.Y / pdet1.Z - arr[7]) * scans[EnergyIndex].det_column_pitch);
            float px = (float)scans[EnergyIndex].det_column_pitch;



            //return null;
            return new float[] {
                px*m1.M11, px*m1.M12, px*m1.M13, px*m1.M14,
                px*m1.M21, px*m1.M22, px*m1.M23, px*m1.M24,
                m1.M31, m1.M32, m1.M33, m1.M34 };
        }

        /// <summary>
        /// scrive nell'array angoli il valore degli angoli nella scansione zero
        /// </summary>
        public void write_to_angles_array(ref float[] arr_ang, ref float[] arrsin, ref float[] arrcos, int energyIndex)
        {
            arr_ang = new float[scans[energyIndex].angle_eff.Length];
            arrsin = new float[scans[energyIndex].angle_eff.Length];
            arrcos = new float[scans[energyIndex].angle_eff.Length];
            for (int i = 0; i < scans[energyIndex].angle_eff.Length; i++)
            {
                arr_ang[i] = (float)(-Math.PI / 180.0 * scans[energyIndex].angle_eff[i]);
                arrsin[i] = (float)(Math.Sin(arr_ang[i]));
                arrcos[i] = (float)(Math.Cos(arr_ang[i]));
            }
        }
        public void GetI0(int index_energy, int nimg, ref double[] vaI0)
        {
            if (scans[index_energy].I0 == null)
                MakeI0();
            vaI0[index_energy] = scans[index_energy].I0[nimg];
        }
        public double[] GetI0(int index_energy)
        {
            if (scans[index_energy].I0 == null)
                MakeI0();
            return scans[index_energy].I0;
        }
        public enum FOV_pxdim { empty = 0, fov17x17_308um = 1, fov15x06_154um = 2, fov10x10_154um = 3 }

        Dictionary<FOV_pxdim, Dictionary<decimal, I0_calculator>> I0CalculatorDictionary = new Dictionary<FOV_pxdim, Dictionary<decimal, I0_calculator>>()
        {
            {FOV_pxdim.fov17x17_308um,new Dictionary<decimal, I0_calculator>(){ 
                //KV                    default I0     (y = Ax+B)    A           B
                { 120, new I0_calculator(42000,new line_equation(3131.556, -374.043)) },
                {80, new I0_calculator(40000,new line_equation(1395.841, -315.374)) } }
            },
            {FOV_pxdim.fov15x06_154um,new Dictionary<decimal, I0_calculator>(){
                { 120, new I0_calculator(31000,new line_equation(8020.34, -222.125)) },
                {80, new I0_calculator(29000,new line_equation(3768.585, -576.258)) } }
            },
            {FOV_pxdim.fov10x10_154um,new Dictionary<decimal, I0_calculator>(){
                { 120, new I0_calculator(31000,new line_equation(8020.34, -222.125)) },
                {80, new I0_calculator(29000,new line_equation(3768.585, -576.258)) } }
            },
            //se non ho i dati per il dapmetro uso questa entrata del dizionario settando il FOV a empty e i kev a 0
            {FOV_pxdim.empty,new Dictionary<decimal, I0_calculator>(){
                { 0, new I0_calculator(40000,null )}}
            }
        };

        protected void MakeI0()
        {
            for (int i = 0; i < scans.Length; i++)
            {
                string folderPath = Path.GetDirectoryName(json_path) + "\\imgScan_" + i.ToString();

                if (!Directory.Exists(folderPath))
                    throw new Exception("Manca la cartella immagini raw: " + folderPath);

                Fov = GetFOV_pixdim(fov, scans[i].det_column_pitch);

                decimal kev = Fov == FOV_pxdim.empty ? 0 : scans[i].kv;//se non ho il fov metto a zero i kev e calcolo I0 dalle immagini, vedi nella classe I0calculator
                if (!I0CalculatorDictionary[Fov].TryGetValue(kev, out var I0calc))
                    I0CalculatorDictionary[FOV_pxdim.empty].TryGetValue(0, out I0calc);//se non trovo i kev calcolo I0 dalle immagini

                scans[i].I0 = I0calc.Evaluate(scans[i], folderPath);
                //questo sará da controllare/eliminare
                if (MaxScatterFraction == 0)
                {
                    if (scans[i].MaxScatterFraction == 0)
                    {
                        switch (Fov)
                        {
                            case FOV_pxdim.empty:
                                break;
                            case FOV_pxdim.fov17x17_308um:
                                scans[i].MaxScatterFraction = 1.65;
                                break;
                            case FOV_pxdim.fov15x06_154um:
                                scans[i].MaxScatterFraction = 1.4;
                                break;
                        }

                    }
                }
            }

        }



        public static Cefla_de_data ReadFromJson(string json_path)
        {

            //Cefla_de_data tdati_cefla = JsonConvert.DeserializeObject<Cefla_de_data>(System.IO.File.ReadAllText(json_path, System.Text.Encoding.GetEncoding("iso-8859-1")));
            Cefla_de_data tdati_cefla = new Cefla_de_data();
            try
            {
                tdati_cefla = JsonConvert.DeserializeObject<Cefla_de_data>(System.IO.File.ReadAllText(json_path, System.Text.Encoding.GetEncoding("iso-8859-1")));//questo pare funzionare per CEFLA
                //tdati_cefla = JsonConvert.DeserializeObject<Cefla_de_data>(System.IO.File.ReadAllText(json_path, System.Text.Encoding.UTF8));
            }
            catch (Exception ex)
            {
                log.Error(ex.Message);
            }

            tdati_cefla.Init(json_path);
            return tdati_cefla;

        }
        public void Init(string json_path)
        {
            this.json_path = json_path;
            //arrotondo la dimensione dei voxel a 5 cifre decimali
            for (int i = 0; i < scans.Length; i++)
            {
                scans[i].vol_voxel_size = scans[i].vol_voxel_size.Select(t => Math.Round(t, 5)).ToArray();
                scans[i].det_row_pitch = Math.Round(scans[i].det_row_pitch, 5);
                scans[i].det_column_pitch = Math.Round(scans[i].det_column_pitch, 5);
            }
            VET = animal_species != null;
            MakeI0();
        }

        private FOV_pxdim GetFOV_pixdim(string fov_str, double pxdim)
        {

            decimal c1 = 0, c2 = 0;
            var tstr = fov_str.Replace("[", "");
            tstr = tstr.Replace("]", "");
            var strarr = tstr.Split('x');
            if (strarr.Length != 2) { return FOV_pxdim.empty; }
            decimal.TryParse(strarr[0], out c1);
            decimal.TryParse(strarr[1], out c2);
            if (c1 == 17 && c2 == 17)
            {
                if (Math.Abs(pxdim - 0.308) < 0.0001)
                    return FOV_pxdim.fov17x17_308um;
            }
            if (c1 == 15 && c2 == 6)
            {
                if (Math.Abs(pxdim - 0.154) < 0.0001)
                    return FOV_pxdim.fov15x06_154um;
            }
            if (c1 == 10 && c2 == 10)
            {
                if (Math.Abs(pxdim - 0.154) < 0.0001)
                    return FOV_pxdim.fov10x10_154um;
            }
            return FOV_pxdim.empty;

        }
        class line_equation
        {
            double a, b;
            public line_equation(double a, double b)
            {
                this.a = a;
                this.b = b;
            }
            public double Evaluate(double x) { return a * x + b; }
            public double[] Evaluate(double[] arr)
            {
                var tmp = this;

                return arr?.Select(x => tmp.Evaluate(x)).ToArray();
            }

        }
        class I0_calculator
        {
            double defaultI0;
            line_equation leq;
            public I0_calculator(double defaultI0, line_equation leq)
            {
                this.defaultI0 = defaultI0;
                this.leq = leq;
            }
            public double[] Evaluate(cefla_scan_data scan, string folderPath)
            {
                var I0FromDap = leq?.Evaluate(GetDapValueForEveryImage(scan));
                var I0FromImages = CalcI0fromImages(scan, folderPath, defaultI0);
                if (I0FromDap == null)
                {
                    log.Warn("Missing DAP information, I0 calculated from images");
                    return I0FromImages;

                }
                for (int i = 0; i < I0FromDap.Length; i++)
                {
                    log.Warn("DAP Loaded");
                    if (Math.Abs(I0FromDap[i] - I0FromImages[i]) > 3500)
                    {
                        log.Warn("DAP calculated I0 differs too much from images I0, I0 calculated from images will be used instead");
                        I0FromDap[i] = I0FromImages[i];
                    }
                }
                return I0FromDap;
            }

            private double[] GetDapValueForEveryImage(cefla_scan_data scan)
            {
                if (scan.dap_value == null)
                    return null;
                var dapv = scan.dap_value;
                var bindap = ArrayBin(dapv, 50, out var pos);
                var dapv_smooth = new double[scan.angle_eff.Length];
                MathNet.Numerics.Interpolation.CubicSpline model = MathNet.Numerics.Interpolation.CubicSpline.InterpolateAkimaSorted(pos, bindap);
                for (int i = 0; i < dapv_smooth.Length; i++)
                {
                    dapv_smooth[i] = model.Differentiate(i);
                }
                return dapv_smooth;
            }
            private double[] CalcI0fromImages(cefla_scan_data scan, string folderPath, double defaultI0)
            {
                if (Directory.GetFiles(folderPath, "*_Img*.raw").Length <= 0)
                {
                    ErrorReport.ErrorLog("ERROR 0077 - No files found.");
                    MessageBox.Show("ERROR 0077 - No files found.");
                    throw new Exception("Carella immagini vuota: " + folderPath);
                }
                string[] allFiles = Directory.GetFiles(folderPath, "*_Img*.raw");
                int allFiles_Length = allFiles.Length;
                var stlow = new Image<Gray, ushort>(scan.det_columns, scan.det_rows);
                List<double> airFound = new List<double>();
                for (int i = 0; i < allFiles_Length; i++)
                {
                    if (Math.Abs(scan.angle_eff[i] - 90) < 3 || Math.Abs(scan.angle_eff[i] - 270) < 3)
                    {
                        var tmp = allFiles[i];
                        var tmp1 = System.IO.File.ReadAllBytes(allFiles[i]);


                        stlow.Bytes = System.IO.File.ReadAllBytes(allFiles[i]);
                        airFound.Add(GetI0FromImage(stlow, defaultI0));
                    }
                }
                double I0 = airFound.Average();
                return scan.angle_eff.Select(t => I0).ToArray();
            }
            private double GetI0FromImage(Image<Gray, ushort> OCVImageLoc, double Fcontrol)
            {


                int maxX = OCVImageLoc[0].Rows;
                int maxY = OCVImageLoc[0].Cols;

                long Airloc = 0;


                // contatore per definire il divisore della media airlevel         

                //_Airmax = 0;
                int _CONT = 0;
                double airlev = 0;

                OCVImageLoc.MinMax(out double[] min, out double[] max, out var xymin, out var xymax);

                for (int i = 0; i < maxX; i++)
                {
                    for (int iy = 0; iy < maxY; iy++)
                    {
                        if (OCVImageLoc.Data[i, iy, 0] > (max[0] - 200) && Airloc < 5000000)
                        {
                            Airloc += (ushort)OCVImageLoc.Data[i, iy, 0];
                            _CONT++;

                        }
                    }
                }

                if ((Int16)Fcontrol != 0)
                {
                    Airloc = (long)((float)Airloc / (float)_CONT);

                    if (Airloc < Fcontrol * .8f)
                        airlev = (long)Fcontrol;
                    else
                        airlev = Airloc;
                }
                else
                {
                    airlev = Airloc;
                }

                return airlev;

            }
        }

        public bool CEFLAScatterCorrectionExists(FBP.ReconstructionType reconstructionType)
        {
            switch (reconstructionType)
            {
                case FBP.ReconstructionType.SingleEnergy:
                    if (scans[0].scatterGLToSub != null && scans[0].scatterFraction != null && scans[0].scatterGLToSub.Length > 0 && scans[0].scatterFraction.Length > 0)
                        return true;
                    log.Warn("CEFLA scatter params for hi energy are missing");
                    break;
                case FBP.ReconstructionType.VMI:
                    return CEFLAScatterCorrectionExists(FBP.ReconstructionType.SingleEnergy) && CEFLAScatterCorrectionExists(FBP.ReconstructionType.SingleEnergyLow);
                case FBP.ReconstructionType.SingleEnergyLow:
                    if (scans[1].scatterGLToSub != null && scans[1].scatterFraction != null && scans[1].scatterGLToSub.Length > 0 && scans[1].scatterFraction.Length > 0)
                        return true;
                    log.Warn("CEFLA scatter params for low energy are missing");

                    break;
            }
            return false;
        }
        internal static double[] ArrayBin(double[] arr, int bin, out double[] pos)
        {
            int l = arr.Length;
            int l1 = l / bin;
            double[] arr_bin = new double[l1];
            pos = new double[l1];

            for (int i = 0; i < l1; i++)
            {
                double cnt = 0;
                for (int j = 0; j < bin; j++)
                {
                    if (i * bin + j < l)
                    {
                        arr_bin[i] += arr[i * bin + j];
                        pos[i] += i * bin + j;
                        cnt++;
                    }
                }
                arr_bin[i] /= cnt;
                pos[i] /= cnt;
            }
            return arr_bin;
        }

    }


    public class cefla_scan_data
    {
        public double[] air_kerma;
        public double[] angle_eff;
        public double[] angle_nom;
        public double[] anodic_current;
        public double[] iso_u;
        public double[] iso_v;
        public double[] ortho_u;
        public double[] ortho_v;
        public double[] sid;
        public double[] skew;
        public double[] slant;
        public double[] sod;
        public double[] tilt;
        public double[] vol_midpoint_translation;
        public double[] vol_size;
        public double[] vol_voxel_size;
        public double[] z_sorg;
        public double[] dap_value;
        public double[][] matrix_proj;
        public double blank_air_kerma;
        public double blank_anodic_current;
        public double det_byte_pixel;
        public double det_column_pitch;
        public int det_columns;
        public int det_num_proj;
        public double det_row_pitch;
        public int det_rows;
        public decimal kv;
        public double MaxScatterFraction;
        /// <summary>
        /// uncorrected value, use GetScatterGLConstantNNT(int index)
        /// </summary>
        public double[] scatterGLToSub;
        public double[] scatterFraction;
        [JsonIgnore]
        public double[] I0;
        private double[] actualGlToSub;
        private static readonly log4net.ILog log = log4net.LogManager.GetLogger(System.Reflection.MethodBase.GetCurrentMethod().DeclaringType);

        public double GetScatterGLConstantNNT(int index)
        {
            //inizializzazione array con correzione spiegata nelle email di simone
            if (actualGlToSub == null)
            {
                actualGlToSub = new double[angle_eff.Length];
                if (scatterGLToSub != null)
                {
                    //questo non dovrebbe mai succedere ma vabbé
                    if (MaxScatterFraction == 0) MaxScatterFraction = 20;//cosí viene bypassata
                                                                         //prendo il massimo valore di scatterFraction
                    double fMaxValue = scatterFraction.Max();
                    double factor = fMaxValue > (MaxScatterFraction - 1) ? (MaxScatterFraction - 1) / fMaxValue : 1;
                    var tmpActualGlToSub = scatterGLToSub.Select(x => x * factor).ToArray();
                    //tolgo lo smoothing se ho pochi punti
                    var bindap = new double[0];
                    var pos = new double[0];
                    if (tmpActualGlToSub.Length == angle_eff.Length)
                    {
                        bindap = Cefla_de_data.ArrayBin(tmpActualGlToSub, 20, out pos);
                        log.Info("Scatter CEFLA PARAMS OK at energy: " + kv.ToString() + "KV, smoothing 20 applied");
                    }
                    else if (tmpActualGlToSub.Length > angle_eff.Length)
                    {
                        log.Warn($"Scatter CEFLA PARAMS are more than the number of images at: {kv} KV tmpActualGlToSub.Length {tmpActualGlToSub.Length}, angle_eff.Length {angle_eff.Length}");
                        bindap = Cefla_de_data.ArrayBin(tmpActualGlToSub, 20, out pos);
                    }
                    else if (tmpActualGlToSub.Length > angle_eff.Length/2)
                    {
                        log.Warn($"Scatter CEFLA PARAMS are less than the number of images at: {kv} KV tmpActualGlToSub.Length {tmpActualGlToSub.Length}, angle_eff.Length {angle_eff.Length}");
                        bindap = Cefla_de_data.ArrayBin(tmpActualGlToSub, 10, out pos);
                    }
                    else//non dovrebbe mai succedere
                    {
                        log.Error($"Scatter CEFLA PARAMS TOO FEW at: {kv} KV tmpActualGlToSub.Length {tmpActualGlToSub.Length}, angle_eff.Length {angle_eff.Length} ");
                        actualGlToSub = new double[1];
                    }
                    MathNet.Numerics.Interpolation.CubicSpline model = MathNet.Numerics.Interpolation.CubicSpline.InterpolateAkimaSorted(pos, bindap);
                    actualGlToSub = actualGlToSub.Select((x, i) => model.Interpolate(i)).ToArray();
                    //Console.WriteLine("scatter " + kv.ToString(ci) + "KV");
                    //for (int i = 0; i < actualGlToSub.Length; i++)
                    //{
                    //    Console.WriteLine(actualGlToSub[i]);
                    //}
                }
            }
            double scatterValue = index < actualGlToSub.Length ? actualGlToSub[index] : 0;
            return scatterValue;
        }


    }
}
