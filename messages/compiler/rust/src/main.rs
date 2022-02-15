use std::error::Error;
use std::io::Cursor;
use std::io::Read;

use byteorder::{BigEndian, ReadBytesExt};
use duplicate::duplicate;
use num_enum::IntoPrimitive;

trait Serializable {
  fn serialize(&self, output: &mut Vec<u8>); 
}

trait Deserializable {
  fn deserialize(&mut self, input: &mut Cursor<Vec<u8>>) -> Result<(), Box<dyn Error>>;
}

impl Serializable for bool {
  fn serialize(&self, output: &mut Vec<u8>) {
    let byte = if *self == true { 1 } else { 0 };
    output.push(byte);
    println!("{:?}", output);
  }
}

#[duplicate(int_type; 
  [i8]; [i16]; [i32]; [i64];
  [u8]; [u16]; [u32]; [u64])]
impl Serializable for int_type {
  fn serialize(&self, output: &mut Vec<u8>) {
    output.extend_from_slice(&self.to_be_bytes());
    println!("{}", std::any::type_name::<int_type>());
  }
}

impl Serializable for String {
  fn serialize(&self, output: &mut Vec<u8>) {
    let t = self.as_bytes();
    let l : u32 = t.len().try_into().unwrap();

    l.serialize(output);
    output.extend_from_slice(&t);
    println!("{:?}", output);
  }
}

impl Deserializable for bool {
  fn deserialize(&mut self, input: &mut Cursor<Vec<u8>>) -> Result<(), Box<dyn Error>> {
    let value = input.read_u8()?;
    *self = if value == 1 { true } else { false };
    return Ok(());
  }
}

#[duplicate(
  int_type read_func;
  [i8]  [read_i8];
  [i16] [read_i16::<BigEndian>];
  [i32] [read_i32::<BigEndian>];
  [i64] [read_i64::<BigEndian>];

  [u8]  [read_u8];
  [u16] [read_u16::<BigEndian>];
  [u32] [read_u32::<BigEndian>];
  [u64] [read_u64::<BigEndian>];
)]
impl Deserializable for int_type {
  fn deserialize(&mut self, input: &mut Cursor<Vec<u8>>) -> Result<(), Box<dyn Error>> {
    *self = input.read_func()?;
    return Ok(());
  }
}

impl Deserializable for String {
  fn deserialize(&mut self, input: &mut Cursor<Vec<u8>>) -> Result<(), Box<dyn Error>> {
    let mut len : u32 = 0;
    len.deserialize(input)?;
    let mut adapter = input.take(len.into());
    
    let mut v = vec![0; len.try_into().unwrap()];
    adapter.read(&mut v);
    *self = String::from_utf8_lossy(&v).to_string();
    println!("{}", *self);
    return Ok(());
  }
}

fn serialize_deserialize_test() {
  let b_true = true;
  let b_false = false;

  let i_min = i8::MIN;
  let i_max = i64::MAX;
  let u_min = u8::MIN;
  let u_max = u64::MAX;

  let s = String::from("test");

  let mut v = vec!();
  let mut v_string = vec!();
  b_true.serialize(&mut v);
  b_false.serialize(&mut v);
  
  i_min.serialize(&mut v);
  i_max.serialize(&mut v);
  u_min.serialize(&mut v);
  u_max.serialize(&mut v);
  
  s.serialize(&mut v_string);

  let mut c = Cursor::new(v_string);
  
  let mut t = String::new();
  t.deserialize(&mut c);
}

#[derive(IntoPrimitive)]
#[repr(u8)]
enum CppEnum {
  E1,
  E2,
  E3,
}

fn enum_test() {
  let mut v = vec!();
  let i: u32 = 42;
  v.extend_from_slice(&i.to_be_bytes());

  let e = CppEnum::E2;
  v.push(e.into());

  let mut rdr = Cursor::new(v);
  println!("{}", rdr.read_u32::<BigEndian>().unwrap());
  println!("{}", rdr.read_u8().unwrap());
}

struct V1 {
  x: u32
}

impl V1 {
  const id: u32 = 1;
}

impl Serializable for V1 {
  fn serialize(&self, output: &mut Vec<u8>) {
    self.x.serialize(output);
  }
}

struct V2 {}

impl V2 {
  const id: u32 = 2;
}

enum CppVariant {
  v1(V1),
  v2(V2),
}

fn variant_test() {
  let mut vec = vec!();
  let variant = CppVariant::v1(V1{x: 15});
  match variant {
    CppVariant::v1(var) => {
      V1::id.serialize(&mut vec);
      var.serialize(&mut vec);
    },
    _ => ()
  }
}

fn main() {
  enum_test();
  variant_test();
  serialize_deserialize_test();

}