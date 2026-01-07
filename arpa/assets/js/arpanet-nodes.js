// ARPANET Nodes Array and Interactive Map Functionality

const arpanetNodes = [

  { name: 'SRI - PDP-10', name1: 'SRI', name2: 'PDP-10', x: 177, y: 92, rx: 33, ry: 18, 		host: 2, 	hostname: 'SRI-ARC', system: 'TENEX, NLS', status: 'dedicated Server',  scenario: 'SRI-ARC (NIC) ==• HOST #2', scenstat: 1  },
  { name: 'SRI - PDP-10', name1: 'SRI', name2: 'PDP-10', x: 98, y: 197, rx: 30, ry: 15, 		host: 66, 	hostname: 'SRI-AI', system: 'TENEX', status: 'limited Server' },
  { name: 'SRI - PDP-15', name1: 'SRI', name2: 'PDP-15', x: 98, y: 150, rx: 30, ry: 15, 		host: 66, 	hostname: 'SRI-AI', system: '?', status: '?', front: 1 },
  { node: 2, modem1: 16, modem2: 34, modem3: 32, name: 'SRI - IMP', name1: 'SRI', name2: 'IMP', x: 177, y: 150, rx: 18, ry: 13,		host: 2, lat: 37.4564811, lon: -122.1753924 },

  { name: 'XEROX - MAXC', name1: 'XEROX', name2: 'MAXC', x: 309, y: 194, rx: 28, ry: 13, 	host: 32, 	hostname: 'PARC-MAXC', system: 'TENEX', status: 'limited Server' },
  { name: 'XEROX - NOVA', name1: 'XEROX', name2: 'NOVA', x: 309, y: 229, rx: 28, ry: 13, 	host: 96, 	hostname: 'PARC-VTS', computer: 'Nova 800', status: 'User' },
  { node: 32, modem1: 2, modem2: 33, name: 'XEROX - 316 IMP', name1: 'XEROX', name2: '316 IMP', x: 238, y: 227, rx: 25, ry: 13, host: 32},

  { name: 'AMES - 360/67', name1: 'AMES', name2: '360/67', x: 98, y: 303, rx: 33, ry: 15, 									host: 144, 	hostname: 'AMES-67', computer: 'IBM 360/67', status: 'Server' },
  { name: 'AMES - TIP', name1: 'AMES', name2: 'TIP', x: 182, y: 303, rx: 18, ry: 13, 										host: 80, 	hostname: 'AMES-TIP', computer: 'TIP', status: 'TIP' },
  { node: 16, modem1: 2, modem2: 11, modem3: 36, name: 'AMES - IMP', name1: 'AMES', name2: 'IMP', x: 182, y: 506, rx: 18, ry: 13,		host: 16 },
  { name: 'AMES - PDP-10', name1: 'AMES', name2: 'PDP-10', x: 255, y: 507, rx: 18, ry: 13, 									host: 16, hostname: '?', computer: '?', status: '?' },

  { node: 36, modem1: 16, name: 'HAWAII - TIP', name1: 'HAWAII', name2: 'TIP', x: 59, y: 506, rx: 22, ry: 12, 		host: 164, 	hostname: 'ALOHA-TIP', computer: 'TIP', status: 'TIP' },

  { node: 33, modem1: 32, modem2: 3, name: 'FNWC - TIP', name1: 'FNWC', name2: 'TIP', x: 262, y: 378, rx: 22, ry: 12, 		host: 161, 	hostname: 'FNWC-TIP', computer: 'TIP', status: 'TIP' },

  { node: 34, modem1: 2, modem2: 4, name: 'LBL - 316 IMP', name1: 'LBL', name2: '316 IMP', x: 451, y: 150, rx: 25, ry: 13, 	host: 34, lat: 37.8769588, lon: -122.2456303 },

  { name: 'UTAH - PDP-10', name1: 'UTAH', name2: 'PDP-10', x: 587, y: 92, rx: 33, ry: 18, 	host: 4, 	hostname: 'UTAH-10', computer: 'PDP-10', system: 'TENEX', status: 'limited Server' },
  { node: 4, modem1: 34, modem2: 12, name: 'UTAH - IMP', name1: 'UTAH', name2: 'IMP', x: 583, y: 150, rx: 18, ry: 13, 		host: 4, lat: 40.7628137, lon: -111.8368719 },

  { name: 'ILLINOIS - PDP-11', name1: 'ILLINOIS', name2: 'PDP-11', x: 708, y: 92, rx: 35, ry: 18, host: 12, hostname: 'ILL-CAC', computer: 'PDP-11/20', system: 'ANTS', status: 'User' },
  { node: 12, modem1: 4, modem2: 6, name: 'ILLINOIS - IMP', name1: 'ILLINOIS', name2: 'IMP', x: 713, y: 150, rx: 18, ry: 13, lat: 40.1131564, lon: -88.2247919 },

  { name: 'MIT - PDP-10 (DMS)', name1: 'MIT', name2: 'PDP-10 (DMS)', x: 1010, y: 76, rx: 33, ry: 18, host: 70, hostname: 'MIT-DMS', computer: 'PDP-10', system: 'ITS', status: 'Server', scenario: 'MIT-DMS ITS PDP-10 — HOST #70', scenstat: 0 },
  { name: 'MIT - PDP-10 (AI)', name1: 'MIT', name2: 'PDP-10 (AI)', x: 940, y: 106, rx: 33, ry: 18,  host: 134, hostname: 'MIT-AI', computer: 'PDP-10', system: 'ITS', status: 'Server', scenario: 'MIT-AI PDP-10 === HOST #134', scenstat: 0  },
  { name: 'MIT - PDP-10 (ML)', name1: 'MIT', name2: 'PDP-10 (ML)', x: 1095, y: 106, rx: 33, ry: 18, host: 198, hostname: 'MIT-ML', computer: 'PDP-10', system: 'ITS', status: 'Server', scenario: 'MIT-ML === HOST #198', scenstat: 0  },
  { node: 6, modem1: 12, modem2: 31, modem3: 10, name: 'MIT - IMP', name1: 'MIT', name2: 'IMP', x: 1010, y: 150, rx: 18, ry: 13, 		host: 6, lat: 42.3582529, lon: -71.0966272 },
  { name: 'MIT - H-645', name1: 'MIT', name2: 'H-645', x: 1091, y: 164, rx: 28, ry: 13, 	host: 6, hostname: 'MIT-MULTICS', computer: 'H-6180', system: 'Multics', status: 'Server', scenario: 'MIT H645 MULTICS === HOST #6', scenstat: 2  },

  { name: 'LINCOLN - TX-2', name1: 'LINCOLN', name2: 'TX-2', x: 785, y: 193, rx: 28, ry: 15, 	host: 74, hostname: 'LL-TX2', computer: 'TX-2', status: 'Server' },
  { name: 'LINCOLN - 360/67', name1: 'LINCOLN', name2: '360/67', x: 868, y: 189, rx: 42, ry: 13, host: 10, hostname: 'LL-67', computer: 'IBM 360/67', status: 'limited Server' },
  { name: 'LINCOLN - TSP', name1: 'LINCOLN', name2: 'TSP', x: 784, y: 251, rx: 22, ry: 12, 	host: 138, hostname: 'LL-TSP', computer: 'TSP', status: 'User' },
  { node: 10, modem1: 6, modem2: 18, name: 'LINCOLN - IMP', name1: 'LINCOLN', name2: 'IMP', x: 866, y: 243, rx: 22, ry: 12,	host: 10, lat: 42.4571974, lon: -71.2668016 },

  { node: 31, modem1: 6, modem2: 5, modem3: 62, name: 'CCA - TIP', name1: 'CCA', name2: 'TIP', x: 1009, y: 219, rx: 25, ry: 12, 		host: 159, hostname: 'CCA-TIP', computer: 'TIP', status: 'TIP' },
  { name: 'CCA - PDP-10', name1: 'CCA', name2: 'PDP-10', x: 1095, y: 225, rx: 33, ry: 18, 	host: 31, hostname: 'CCA-TENEX', computer: 'PDP-10', system: 'TENEX, Datacomputer', status: 'dedicated Server' },



//this is for testing, remove together with modem3:31 line in CCA above:
  { node: 62, modem1: 31, name: 'HILTON - IMP', name1: 'HILTON', name2: 'IMP', x: 10, y: 10, rx: 25, ry: 12, 		host: 62 },
  { name: 'HILTON-KA1', name1: 'HILTON', name2: 'KA', x: 10, y: 10, rx: 33, ry: 18, 	host: 126, hostname: 'HILTON-KA1', computer: 'PDP-10', system: 'ITS', status: 'Server' },
  { name: 'HILTON-KA0', name1: 'HILTON', name2: 'KA', x: 10, y: 10, rx: 33, ry: 18, 	host: 62, hostname: 'HILTON-KA0', computer: 'PDP-10', system: 'ITS', status: 'Server' },
  
  
  
  

  { node: 5, modem1: 31, modem2:9, modem3: 30, name: 'BBN - IMP', name1: 'BBN', name2: 'IMP', x: 1016, y: 340, rx: 18, ry: 13,			host: 5, lat: 42.3908323, lon: -71.1469589 },
  { name: 'BBN - H316', name1: 'BBN', name2: 'H316', 						x: 1095, y: 374, rx: 28, ry: 15, 							host: 5, hostname: 'BBN-NCC', computer: 'H-316', system: 'H-316', status: 'User' },
  { name: 'BBN - PDP-10 (TENEX)', name1: 'BBN', name2: 'PDP-10 (TENEX)', 	x: 967, y: 279, rx: 33, ry: 15, 							host: 69,  hostname: 'BBN-TENEX', computer: 'PDP-10', system: 'TENEX', status: 'Server', scenario: 'BBN TENEX === HOST #69', scenstat: 1  },
  { name: 'BBN - PDP-10 (TENEXB)', name1: 'BBN', name2: 'PDP-10 (TENEXB)', 	x: 970, y: 389, rx: 33, ry: 15, 							host: 133, hostname: 'BBN-TENEXB', computer: 'PDP-10', system: 'TENEX', status: 'limited Server' },
  { name: 'BBN - PDP-1', name1: 'BBN', name2: 'PDP-1', 						x: 1095, y: 309, rx: 30, ry: 15, 							host: 197, hostname: 'BBN-1D', computer: 'PDP-1', system: 'PDP-1 TS', status: 'User' },

  { node: 30, modem1: 5, name: 'BBN2 - TIP', name1: 'BBN2', name2: 'TIP', 	x: 942, y: 340, rx: 22, ry: 12, 		host: 158, hostname: 'BBN-TESTIP', computer: 'TIP', status: 'TIP'},

  { node: 9, modem1: 5, modem2: 29, name: 'HARVARD - PDP-11', name1: 'HARVARD', name2: 'PDP-11', x: 930, y: 493, rx: 35, ry: 15, host: 137, hostname: 'HARV-11', computer: 'PDP-11', status: 'User' },
  { name: 'HARVARD - PDP-10', name1: 'HARVARD', name2: 'PDP-10', x: 930, y: 437, rx: 35, ry: 15, host: 9, hostname: 'HARV-10', computer: 'PDP-10', status: 'Server', scenario: 'HARVARD PDP-10 === HOST #9', scenstat: 1  },
  { name: 'HARVARD - IMP', name1: 'HARVARD', name2: 'IMP', x: 1014, y: 488, rx: 18, ry: 13, lat: 42.3657432, lon: -71.1222139 },
  { name: 'HARVARD - PDP-1', name1: 'HARVARD', name2: 'PDP-1', x: 1095, y: 488, rx: 30, ry: 15, host: 73, hostname: 'HARV-1', computer: 'PDP-1', status: 'User' },


  { node: 29, modem1: 9, modem2: 19, modem3: 27, name: 'ABERDEEN - 316 IMP', name1: 'ABERDEEN', name2: '316 IMP', x: 1014, y: 565, rx: 25, ry: 13,	host: 29 },


  { node: 19, modem1: 29, modem2: 20, name: 'NBS - TIP', name1: 'NBS', name2: 'TIP', x: 1014, y: 631, rx: 18, ry: 13, 		host: 147, hostname: 'NBS-TIP', computer: 'TIP', status: 'TIP' },
  { name: 'NBS - PDP-11', name1: 'NBS', name2: 'PDP-11', x: 1099, y: 631, rx: 30, ry: 15, 	host: 19, hostname: 'NBS-ICST', computer: 'PDP-11/45', system: 'ANTS', status: 'User' },

  { node: 20, modem1: 19, modem2: 37, modem3: 28, name: 'ETAC - TIP', name1: 'ETAC', name2: 'TIP', x: 1014, y: 825, rx: 22, ry: 12, 		host: 148, hostname: 'ETAC-TIP', computer: 'TIP', status: 'TIP' },

  { node: 28, modem1: 20, modem2: 17, name: 'ARPA - TIP', name1: 'ARPA', name2: 'TIP', x: 981, y: 757, rx: 22, ry: 12, 		host: 156, hostname: 'ARPA-TIP', computer: 'TIP', status: 'TIP' },
  { name: 'ARPA - PDP-15', name1: 'ARPA', name2: 'PDP-15', x: 898, y: 757, rx: 30, ry: 15, 	host: 28, hostname: 'ARPA-DMS', computer: 'PDP-15', status: 'User' },

  { node: 26, modem1: 17, modem2: 27, name: 'SDAC - TIP', name1: 'SDAC', name2: 'TIP', x: 920, y: 628, rx: 22, ry: 12, 		host: 154, hostname: 'SDAC-TIP', computer: 'TIP', status: 'TIP' },


  { node: 27, modem1: 26, modem2:14, modem3: 29, name: 'BELVOIR - 316 IMP', name1: 'BELVOIR', name2: '316 IMP', x: 879, y: 563, rx: 25, ry: 13, host: 27 },


  { name: 'CARNEGIE - PDP-10 (B)', name1: 'CARNEGIE', name2: 'PDP-10 (B)', x: 729, y: 458, rx: 40, ry: 13, host: 14, hostname: 'CMU-10B', computer: 'PDP-10', status: 'Server' },
  { name: 'CARNEGIE - PDP-10 (A)', name1: 'CARNEGIE', name2: 'PDP-10 (A)', x: 747, y: 517, rx: 40, ry: 13, host: 78, hostname: 'CMU-10A', computer: 'PDP-10', status: 'Server' },
  { node:14, modem1: 27, modem2: 13, name: 'CARNEGIE - IMP', name1: 'CARNEGIE', name2: 'IMP', x: 811, y: 458, rx: 22, ry: 12,		host: 14, lat: 40.4441897, lon: -79.9427192 },

  { node: 13, modem1: 14, modem2: 24, modem3: 18, name: 'CASE - IMP', name1: 'CASE', name2: 'IMP', x: 736, y: 344, rx: 22, ry: 12,		host: 13, lat: 41.501387, lon: -81.6007022 },
  { name: 'CASE - PDP-10', name1: 'CASE', name2: 'PDP-10', x: 652, y: 345, rx: 40, ry: 13, 	host: 13, hostname: 'CASE-10', computer: 'PDP-10', system: 'TENEX', status: 'Server' },

  { node: 18, modem2: 10, modem3: 13, name: 'RADC - TIP', name1: 'RADC', name2: 'TIP', x: 792, y: 312, rx: 22, ry: 12, 		host: 146, hostname: 'RADC-TIP', computer: 'TIP', status: 'TIP' },

  { node: 24, modem1:13, modem2: 25, name: 'GWC - TIP', name1: 'GWC', name2: 'TIP', x: 675, y: 402, rx: 22, ry: 12, 		host: 152, hostname: 'GWC-TIP', computer: 'TIP', status: 'TIP' },

  { node:25, modem1: 24, modem2: 23, name: 'DOCB - TIP', name1: 'DOCB', name2: 'TIP', x: 622, y: 451, rx: 22, ry: 12, 		host: 153, hostname: 'DOCB-TIP', computer: 'TIP', status: 'TIP' },

  { name: 'USC - 360/44', name1: 'USC', name2: '360/44', x: 633, y: 505, rx: 35, ry: 15, 	host: 23, hostname: 'USC-44', computer: 'IBM 360/44', status: 'Server' },
  { node: 23, modem1: 25, modem2: 8, name: 'USC - TIP', name1: 'USC', name2: 'TIP', x: 551, y: 500, rx: 35, ry: 15, 		host: 151, hostname: 'USC-TIP', computer: 'TIP', status: 'TIP' },

  { name: 'SDC - 370/145', name1: 'SDC', name2: '370/145', x: 590, y: 597, rx: 40, ry: 15, 	host: 8, hostname: 'SDC-LAB', computer: 'IBM 370/145', status: 'limited Server' },
  { node: 8, modem1: 23, modem2: 1, name: 'SDC - IMP', name1: 'SDC', name2: 'IMP', x: 496, y: 553, rx: 40, ry: 15,		host: 8 },
  { name: 'SDC - DDP-516', name1: 'SDC', name2: 'DDP-516', x: 588, y: 553, rx: 40, ry: 15,  	host: 8, hostname: '?', computer: 'DDP-516', status: '?', front: 1 },

  { name: 'UCLA - SIGMA7', name1: 'UCLA', name2: 'SIGMA7', x: 255, y: 639, rx: 38, ry: 15, 	host: 1, hostname: 'UCLA-NMC', computer: 'Sigma 7', system: 'SEX', status: 'Server', scenario: 'UCLA-NMC SIGMA-7 === HOST #1', scenstat: 1  },
  { node: 1, modem1: 8, modem2: 3, modem3:35, name: 'UCLA - IMP', name1: 'UCLA', name2: 'IMP', x: 378, y: 643, rx: 22, ry: 12,		host: 1,  lat: 34.0708777, lon: -118.4468503 },
  { name: 'UCLA - 360/91', name1: 'UCLA', name2: '360/91', x: 460, y: 641, rx: 33, ry: 13, 	host: 65, hostname: 'UCLA-CCn', computer: 'IBM 360/91', status: 'Server', scenario: 'HOST #65'  , scenstat: 2 },

  { node: 35, modem1: 1, modem2:7, name: 'UCSD - 316 IMP', name1: 'UCSD', name2: '316 IMP', x: 323, y: 694, rx: 25, ry: 13 },
  { name: 'UCSD - MICRO/810', name1: 'UCSD', name2: 'MICRO/810', x: 408, y: 690, rx: 35, ry: 15, host: 35, hostname: 'UCSD-CC', computer: 'Micro 810 / B6700', status: 'Server', front: 1 },
  { name: 'UCSD - B6700', name1: 'UCSD', name2: 'B6700', x: 409, y: 732, rx: 32, ry: 15, 	host: 35, hostname: 'UCSD-CC', computer: 'B6700', status: 'Server' },

  { name: 'RAND - IBM 1800', name1: 'RAND', name2: 'IBM 1800', x: 321, y: 755, rx: 30, ry: 13,	host: 7, hostname: '?', computer: 'IBM 1800', status: '?' , front: 1 },
  { node: 7, modem1: 35, modem2: 22, name: 'RAND - 316 IMP', name1: 'RAND', name2: '316 IMP', x: 255, y: 748, rx: 25, ry: 13,	host: 7, lat: 34.0095671, lon: -118.4907088 },
  { name: 'RAND - 360/65', name1: 'RAND', name2: '360/65', x: 321, y: 790, rx: 32, ry: 15, 	host: 7, hostname: 'RAND-RCC', computer: 'IBM 370/158', status: 'User' },

  { name: 'USC-ISI - PDP-10', name1: 'USC-ISI', name2: 'PDP-10', x: 98, y: 827, rx: 33, ry: 15, 	host: 86, hostname: 'USC-ISI', computer: 'PDP-10', system: 'TENEX', status: 'Server' },
  { node: 22, modem1: 7, modem2: 11, modem3: 37, name: 'USC-ISI - IMP', name1: 'USC-ISI', name2: 'IMP', x: 174, y: 827, rx: 18, ry: 13,	host: 22 },

  { name: 'UCSB - PDP-11(V)', name1: 'UCSB', name2: 'PDP-11(V)', x: 255, y: 570, rx: 42, ry: 15, host: 67, hostname: 'SCRL', computer: '(VDH)->PDP-11/45', system: 'ANTS', status: 'User' },
  { name: 'UCSB - 360/75', name1: 'UCSB', name2: '360/75', x: 425, y: 570, rx: 33, ry: 13, 	host: 3, hostname: 'UCSB-MOD75', computer: 'IBM 360/75', system: 'OLS', status: 'Server' },
  { node: 3, modem1: 1, modem2: 33, name: 'UCSB - IMP', name1: 'UCSB', name2: 'IMP', x: 342, y: 570, rx: 22, ry: 12,		host: 3, lat: 34.4146025, lon: -119.84581 },

  { name: 'STANFORD - PDP-10', name1: 'STANFORD', name2: 'PDP-10', x: 103, y: 618, rx: 33, ry: 15, host: 11, hostname: 'SU-AI', computer: 'PDP-10', system: 'SAIL', status: 'Server', scenario: 'SAIL AP Hotline === HOST #11', scenstat: 0 },
  { node: 11, modem1: 22, modem2: 16, name: 'STANFORD - IMP', name1: 'STANFORD', name2: 'IMP', x: 181, y: 621, rx: 18, ry: 13,	host: 11, lat: 37.4351796, lon: -122.1746129 },

  { node: 17, modem1: 26, modem2:28, name: 'MITRE - TIP', name1: 'MITRE', name2: 'TIP', x: 949, y: 692, rx: 22, ry: 12, host: 145, hostname: 'MITRE-TIP', computer: 'TIP', status: 'TIP', lat: 42.5052939, lon: -71.2370477 },

  { node: 37, modem1: 20, modem2: 22, name: 'RML - TIP', name1: 'RML', name2: 'TIP', x: 787, y: 825, rx: 22, ry: 12, host: 165, hostname: 'RML-TIP', computer: 'TIP', status: 'TIP' },
];

// Initialize the interactive map
function initArpanetMap() {
  const svg = document.getElementById('arpanet-map-svg');
  const img = document.getElementById('arpanet-map-img');
  const hoverBox = document.getElementById('node-hover-box');
  const debugCoords = document.getElementById('debug-coords');
  let currentHighlight = null;
  let isMouseOverNode = false;
  let isMouseOverHoverBox = false;
  let hideTimeout = null;

  if (!svg || !img) {
    console.error('ARPANET map elements not found');
    return;
  }

  // Add click debugging - show coordinates when clicking anywhere on the container
  const container = img.parentElement;
  container.addEventListener('click', function(e) {
    const imgRect = img.getBoundingClientRect();
    const containerRect = container.getBoundingClientRect();

    // Calculate click position relative to image
    const imgX = e.clientX - imgRect.left;
    const imgY = e.clientY - imgRect.top;

    // Scale to original image dimensions (1177x879)
    const scaleX = 1177 / imgRect.width;
    const scaleY = 879 / imgRect.height;
    const scaledX = Math.round(imgX * scaleX);
    const scaledY = Math.round(imgY * scaleY);

    // Calculate SVG coordinates
    const svgPoint = svg.createSVGPoint();
    svgPoint.x = e.clientX;
    svgPoint.y = e.clientY;
    const svgCoords = svgPoint.matrixTransform(svg.getScreenCTM().inverse());

    // Update debug coords display if the element exists
    if (debugCoords) {
      debugCoords.innerHTML = `
        <strong>PNG Image:</strong> x=${scaledX}, y=${scaledY} (in 1177x879 space) |
        <strong>SVG Overlay:</strong> x=${Math.round(svgCoords.x)}, y=${Math.round(svgCoords.y)} |
        <strong>Displayed:</strong> ${Math.round(imgRect.width)}x${Math.round(imgRect.height)}px
      `;
    }
  });

  // Add transparent background rectangle to capture clicks on empty space
  const bgRect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
  bgRect.setAttribute('x', 0);
  bgRect.setAttribute('y', 0);
  bgRect.setAttribute('width', 1177);
  bgRect.setAttribute('height', 879);
  bgRect.setAttribute('fill', 'transparent');
  bgRect.style.pointerEvents = 'all';
  bgRect.addEventListener('click', function(e) {
    // Remove highlight
    if (currentHighlight) {
      currentHighlight.remove();
      currentHighlight = null;
    }

    // Hide hover box
    if (hoverBox) {
      hoverBox.classList.remove('visible');
      hoverBox.innerHTML = '';
    }
  });
  svg.appendChild(bgRect);

  // Add hover event handlers to the hover box itself
  if (hoverBox) {
    hoverBox.addEventListener('mouseenter', function(e) {
      isMouseOverHoverBox = true;
      if (hideTimeout) {
        clearTimeout(hideTimeout);
        hideTimeout = null;
      }
    });

    hoverBox.addEventListener('mouseleave', function(e) {
      isMouseOverHoverBox = false;
      if (!isMouseOverNode) {
        if (hideTimeout) {
          clearTimeout(hideTimeout);
        }
        hideTimeout = setTimeout(() => {
          if (!isMouseOverNode && !isMouseOverHoverBox) {
            hoverBox.classList.remove('visible');
            hoverBox.innerHTML = '';
          }
        }, 100);
      }
    });
  }

  // Add corner markers for alignment verification
  const corners = [
    { x: 0, y: 0 },       // Top-left
    { x: 1177, y: 0 },    // Top-right
    { x: 0, y: 879 },     // Bottom-left
    { x: 1177, y: 879 }   // Bottom-right
  ];

  corners.forEach(corner => {
    const marker = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
    marker.setAttribute('cx', corner.x);
    marker.setAttribute('cy', corner.y);
    marker.setAttribute('r', 5);
    marker.setAttribute('class', 'corner-marker');
    svg.appendChild(marker);
  });

  // Create clickable regions for each node
  arpanetNodes.forEach(node => {
    const ellipse = document.createElementNS('http://www.w3.org/2000/svg', 'ellipse');
    ellipse.setAttribute('cx', node.x);
    ellipse.setAttribute('cy', node.y);
    ellipse.setAttribute('rx', node.rx);
    ellipse.setAttribute('ry', node.ry);
    ellipse.setAttribute('data-name', node.name);

    // Set color based on scenario and scenstat
    if (!node.scenario) {
      // No scenario: invisible but clickable
      ellipse.style.fill = 'none';
      ellipse.style.stroke = 'none';
    } else {
      // Has scenario: color based on scenstat
      let fillColor, strokeColor;
      if (node.scenstat === 0) {
        // Green
        fillColor = 'rgba(0, 255, 0, 0.1)';
        strokeColor = 'rgba(0, 255, 0, 0.5)';
      } else if (node.scenstat === 1) {
        // Yellow
        fillColor = 'rgba(255, 255, 0, 0.1)';
        strokeColor = 'rgba(255, 255, 0, 0.5)';
      } else if (node.scenstat === 2) {
        // Red
        fillColor = 'rgba(255, 0, 0, 0.1)';
        strokeColor = 'rgba(255, 0, 0, 0.5)';
      } else {
        // Default green if scenstat not specified
        fillColor = 'rgba(0, 255, 0, 0.1)';
        strokeColor = 'rgba(0, 255, 0, 0.5)';
      }
      ellipse.style.fill = fillColor;
      ellipse.style.stroke = strokeColor;
    }

    // Helper function to display node details in hover box
    const displayNodeInfo = (nodeToDisplay, mouseX, mouseY) => {
      let displayHTML = `<div style="font-weight: bold; margin-bottom: 6px;">${nodeToDisplay.name}</div>`;

      // Add additional information if available
      const details = [];
      let impNumber = null;
      if (nodeToDisplay.host) {
        details.push(`Host: ${nodeToDisplay.host}`);
        const hostNumber = Math.floor(nodeToDisplay.host / 64);
        impNumber = nodeToDisplay.host % 64;
        // Only print Host_Number if this is not an IMP (i.e., has hostname or hostNumber !== 0)
        if (nodeToDisplay.hostname || hostNumber !== 0) {
          details.push(`Host_Number: ${hostNumber}, IMP_Number: ${impNumber}`);
        }
      }
      if (nodeToDisplay.hostname) details.push(`Hostname: ${nodeToDisplay.hostname}`);
      if (nodeToDisplay.computer) details.push(`Computer: ${nodeToDisplay.computer}`);
      if (nodeToDisplay.system) details.push(`System: ${nodeToDisplay.system}`);
      if (nodeToDisplay.status) details.push(`Status: ${nodeToDisplay.status}`);

      if (details.length > 0) {
        displayHTML += `<div style="font-size: 0.85em; line-height: 1.4; color: #333;">${details.join('<br>')}</div>`;
      }

      // Add button to node details page if IMP_Number exists
      if (impNumber !== null) {
        const firstWord = nodeToDisplay.name.split(' ')[0];
        displayHTML += `<button onclick="window.location.href='arpa/arpanet-node-${impNumber}-${firstWord}.html'">View Node Details</button>`;
      }

      // Update hover box content
      if (hoverBox) {
        hoverBox.innerHTML = displayHTML;
        hoverBox.classList.add('visible');

        // Position hover box near the mouse/node
        const container = svg.parentElement;
        const containerRect = container.getBoundingClientRect();
        let x = mouseX - containerRect.left + 10;
        let y = mouseY - containerRect.top + 10;

        // Adjust position to keep box in view
        const boxWidth = 300;
        const boxHeight = hoverBox.offsetHeight;

        if (x + boxWidth > container.offsetWidth) {
          x = container.offsetWidth - boxWidth - 10;
        }
        if (y + boxHeight > container.offsetHeight) {
          y = container.offsetHeight - boxHeight - 10;
        }

        hoverBox.style.left = x + 'px';
        hoverBox.style.top = y + 'px';
      }
    };

    // Mouseover: show node details in hover box
    ellipse.addEventListener('mouseover', function(e) {
      e.preventDefault();
      isMouseOverNode = true;

      // Cancel any pending hide
      if (hideTimeout) {
        clearTimeout(hideTimeout);
        hideTimeout = null;
      }

      displayNodeInfo(node, e.clientX, e.clientY);
    });

    // Mouseout: start hiding if mouse not over hover box
    ellipse.addEventListener('mouseout', function(e) {
      e.preventDefault();
      isMouseOverNode = false;

      // Schedule hide if not over hover box
      if (!isMouseOverHoverBox) {
        if (hideTimeout) {
          clearTimeout(hideTimeout);
        }
        hideTimeout = setTimeout(() => {
          if (!isMouseOverNode && !isMouseOverHoverBox && hoverBox) {
            hoverBox.classList.remove('visible');
            hoverBox.innerHTML = '';
          }
        }, 100);
      }
    });

    svg.appendChild(ellipse);
  });
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', initArpanetMap);
} else {
  initArpanetMap();
}
