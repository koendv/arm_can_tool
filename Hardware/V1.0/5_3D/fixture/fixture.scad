/* 
 PCB Assembly Fixture for gluing ZJY154-2828KSWKG03 OLED to ARM CAN TOOL PCB.
 
 This fixture 
  1. HOLDS the 60x100mm PCB securely in place via press-fit bumps
  2. POSITIONS the OLED display accurately for gluing

 Written in OpenSCAD https://openscad.org
 */

eps = 0.01;
$fn = 16;

pcb_length = 100.0;        // X dimension (mm)
pcb_width = 60.0;          // Y dimension (mm)
pcb_thickness = 1.6;       // Z dimension (mm)
pcb_clearance = 0.2;       // Overall clearance for PCB fit (mm)

oled_width = 37.3;         // X dimension (mm)
oled_height = 33.9;        // Y dimension (mm)
oled_tolerance = 0.2;      // Manufacturing tolerance (±mm)
oled_clearance = 0.2;      // Clearance for OLED placement 
oled_chamfer = 1.0;        // For easy insertion
oled_center_y = 42.45;     // Center Y position (from PCB bottom edge at 0)
oled_center_x = 30.48;     // Center X position (from PCB left edge at 0)

cable_width = 12.0;        // Width of flat cable (X direction when extended)
cable_length = 25.0;       // Length of cable relief (Y direction)
cable_clearance = 0.6;     // Clearance for cable
cable_relief_width = cable_width + cable_clearance;
cable_relief_length = cable_length + cable_clearance;

border = 8.0;
base_thickness = 2.0;
bump_radius = 2.5;

module base_plate()
{
    translate([-border, -border, 0])
    cube([pcb_width+2*border, pcb_length+2*border, base_thickness]);
}

module bump() {
    // tapered bump for easy insertion
    translate([0, 0, base_thickness - eps])
    cylinder(h=pcb_thickness, r1=bump_radius, r2=bump_radius*0.8);
}

module bumps(){
    bump_positions = [
        [pcb_width+bump_radius+pcb_clearance/2, pcb_length/8, 0],
        [pcb_width+bump_radius+pcb_clearance/2, pcb_length*7/8, 0],
        [pcb_width/8, pcb_length+bump_radius+pcb_clearance/2, 0],
        [pcb_width*7/8, pcb_length+bump_radius+pcb_clearance/2, 0],
    ];

    for (pos = bump_positions) {
        translate(pos) bump();
    }

    translate([-2*bump_radius-pcb_clearance/2, pcb_length/8-bump_radius, 0])
    cube([bump_radius*2, pcb_length*3/4+2*bump_radius, base_thickness+pcb_thickness]);

    translate([pcb_width/8-bump_radius, -2*bump_radius-pcb_clearance/2, 0])
    cube([pcb_width*3/4+2*bump_radius, bump_radius*2, base_thickness+pcb_thickness]);
}

module oled() {
    oled_cutout_width = oled_width + oled_tolerance + oled_clearance;
    oled_cutout_height = oled_height + oled_tolerance + oled_clearance;

    translate([oled_center_x,oled_center_y,0])
    cube([oled_cutout_height, oled_cutout_width, 3*pcb_thickness], center = true);
    translate([oled_center_x,oled_center_y+oled_width/2+cable_relief_length/2-eps,0])
    cube([cable_relief_width, cable_relief_length, 3*pcb_thickness], center = true);
    
    /* chamfer for oled */
    translate([oled_center_x,oled_center_y,0])
    hull() {
        translate([0, 0, oled_chamfer+eps])
        cube([oled_cutout_height, oled_cutout_width, eps], center = true);
        
        translate([0, 0, -eps])
        cube([oled_cutout_height+2*oled_chamfer, oled_cutout_width+2*oled_chamfer, eps], center = true);
    }
    
    /* chamfer for cable */
    translate([oled_center_x,oled_center_y+oled_width/2+cable_relief_length/2-eps,0])
    hull() {
        translate([0, 0, oled_chamfer+eps])
        cube([cable_relief_width, cable_relief_length, eps], center = true);
        
        translate([0, 0, -eps])
        cube([cable_relief_width+2*oled_chamfer, cable_relief_length+2*oled_chamfer, eps], center = true);
    }
}

module fixture() {
    difference() {
        union() {
            base_plate();
            bumps();
        }
        oled();
    }
}

module drawing() {
    difference() {
        projection()
        base_plate();

        offset(r = -1)
        projection()
        base_plate();
    }

    projection(){
        
        bumps();
        oled();
    }
}

if (true) {
    fixture();
 } else {
    drawing();
}
