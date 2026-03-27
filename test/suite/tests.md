# Test Coverage by Category

### Section 1: Variable Declarations (Tests 1-10)
Focus on variable declaration syntax and initialization.
- **test1.dl**: Simple integer variable declaration
- **test2.dl**: Simple real variable declaration
- **test3.dl**: Simple string variable declaration
- **test4.dl**: Simple boolean variable declaration
- **test5.dl**: Variable declaration without initialization (none type)
- **test6.dl**: Multiple variables in one declaration
- **test7.dl**: Multiple variables with some uninitialized
- **test8.dl**: Variable reassignment with same type
- **test9.dl**: Variable type change (dynamic typing)
- **test10.dl**: Variable shadowing in nested scopes

---

### Section 2: Integer Operations (Tests 11-25)
Comprehensive testing of arithmetic operations on integers and operator precedence.
- **test11.dl**: Integer addition
- **test12.dl**: Integer subtraction
- **test13.dl**: Integer multiplication
- **test14.dl**: Integer division (round down)
- **test15.dl**: Unary plus on integer
- **test16.dl**: Unary minus on integer
- **test17.dl**: Integer comparison: less than
- **test18.dl**: Integer comparison: greater than
- **test19.dl**: Integer comparison: equal
- **test20.dl**: Integer comparison: not equal
- **test21.dl**: Integer comparison: less or equal
- **test22.dl**: Integer comparison: greater or equal
- **test23.dl**: Multiple integer operations in chain (precedence)
- **test24.dl**: Integer with parentheses
- **test25.dl**: Integer division by 1

---

### Section 3: Real Number Operations (Tests 26-35)
Testing floating-point arithmetic and mixed int/real operations.
- **test26.dl**: Real addition
- **test27.dl**: Real subtraction
- **test28.dl**: Real multiplication
- **test29.dl**: Real division
- **test30.dl**: Integer plus real (implicit conversion)
- **test31.dl**: Real plus integer (implicit conversion)
- **test32.dl**: Real comparison
- **test33.dl**: Real equality
- **test34.dl**: Unary minus on real
- **test35.dl**: Real with many decimal places

---

### Section 4: String Operations (Tests 36-42)
String manipulation and concatenation.
- **test36.dl**: Simple string concatenation
- **test37.dl**: String with single quotes
- **test38.dl**: String concatenation with variable
- **test39.dl**: Empty string
- **test40.dl**: Multiple string concatenations
- **test41.dl**: String comparison (equality)
- **test42.dl**: Boolean AND operation

---

### Section 5: Boolean Operations (Tests 43-50)
Logical operations on boolean values.
- **test43.dl**: Boolean AND operation
- **test44.dl**: Boolean AND with false
- **test45.dl**: Boolean OR operation
- **test46.dl**: Boolean OR with both false
- **test47.dl**: Boolean XOR operation
- **test48.dl**: Boolean XOR with same values
- **test49.dl**: Unary NOT on true
- **test50.dl**: Unary NOT on false

---

### Section 6: Type Checking with 'is' Operator (Tests 51-60)
Type checking and type validation.
- **test51.dl**: Type check - integer
- **test52.dl**: Type check - real
- **test53.dl**: Type check - string
- **test54.dl**: Type check - boolean
- **test55.dl**: Type check - none
- **test56.dl**: Type check - array
- **test57.dl**: Type check - tuple
- **test58.dl**: Type check - function
- **test59.dl**: Type mismatch check
- **test60.dl**: Type check after assignment change

---

### Section 7: Array Operations (Tests 61-75)
Array creation, indexing, and manipulation.
- **test61.dl**: Empty array
- **test62.dl**: Array with integers
- **test63.dl**: Array with mixed types
- **test64.dl**: Array access by index (first element)
- **test65.dl**: Array access (second element)
- **test66.dl**: Array access (third element)
- **test67.dl**: Array assignment/modification
- **test68.dl**: Array with single element
- **test69.dl**: Array concatenation
- **test70.dl**: Nested arrays
- **test71.dl**: Access nested array element
- **test72.dl**: Array type check
- **test73.dl**: Associative array - sparse indices
- **test74.dl**: Array modification
- **test75.dl**: Array with function references

---

### Section 8: Tuple Operations (Tests 76-85)
Tuple creation, element access, and manipulation.
- **test76.dl**: Simple tuple with named elements
- **test77.dl**: Tuple element access by name
- **test78.dl**: Tuple with unnamed elements
- **test79.dl**: Tuple element access by index
- **test80.dl**: Mixed named and unnamed tuple elements
- **test81.dl**: Tuple type check
- **test82.dl**: Tuple concatenation
- **test83.dl**: Tuple with nested values
- **test84.dl**: Tuple with mixed types
- **test85.dl**: Tuple access by integer index

---

### Section 9: Conditional Statements (Tests 86-92)
If/then/else and short if form testing.
- **test86.dl**: Simple if statement
- **test87.dl**: If with else
- **test88.dl**: If short form
- **test89.dl**: If short form with false condition
- **test90.dl**: Nested if statements
- **test91.dl**: If with boolean variable
- **test92.dl**: If with comparison

---

### Section 10: Loop Constructs (Tests 93-100)
For loops, while loops, and loop exit mechanisms.
- **test93.dl**: For loop with range
- **test94.dl**: For loop with variable
- **test95.dl**: While loop
- **test96.dl**: Loop with exit
- **test97.dl**: For loop iterating array
- **test98.dl**: Nested loops
- **test99.dl**: Loop with multiple statements
- **test100.dl**: Infinite loop with counter exit

---

### Section 11: Function Operations (Tests 101-120)
Function definition, parameters, recursion, and higher-order functions.
- **test101.dl**: Function with one parameter
- **test102.dl**: Function with multiple parameters
- **test103.dl**: Function with body (multi-statement)
- **test104.dl**: Function returning array
- **test105.dl**: Function returning tuple
- **test106.dl**: Function returning function (higher-order)
- **test107.dl**: Recursive function (factorial)
- **test108.dl**: Function with no explicit return
- **test109.dl**: Function stored in array
- **test110.dl**: Higher order function (function as argument)
- **test111.dl**: Function with conditional logic
- **test112.dl**: Function assigning to variable in body
- **test113.dl**: Multiple function calls (composition)
- **test114.dl**: Function in tuple
- **test115.dl**: Return with value

---

### Section 12: Return Statements (Tests 116-120)
Testing various return statement scenarios.
- **test116.dl**: Return without value
- **test117.dl**: Early return
- **test118.dl**: Return from nested block
- **test119.dl**: Return expression

---

### Section 13: Print Statements (Tests 121-130)
Print statement with various types and expressions.
- **test121.dl**: Print single value
- **test122.dl**: Print multiple values
- **test123.dl**: Print string
- **test124.dl**: Print variable
- **test125.dl**: Print expression
- **test126.dl**: Print array
- **test127.dl**: Print tuple
- **test128.dl**: Print boolean
- **test129.dl**: Print multiple strings
- **test130.dl**: Print with variable modifications

---

### Section 14: Complex Expressions (Tests 131-140)
Operator precedence, complex expressions, and mixed operations.
- **test131.dl**: Mixed arithmetic with precedence
- **test132.dl**: Boolean logic precedence
- **test133.dl**: Comparison chaining simulation
- **test134.dl**: Expression with parentheses
- **test135.dl**: Type conversion in expression
- **test136.dl**: Array element in expression
- **test137.dl**: Tuple element in expression
- **test138.dl**: Function call in expression
- **test139.dl**: Complex nested expression
- **test140.dl**: Logical operators combination

---

### Section 15: Complex Integrated Tests (Tests 141-150)
Real-world algorithms and complex scenarios combining multiple features.
- **test141.dl**: Sum array elements using loop
- **test142.dl**: Count loop iterations
- **test143.dl**: Find maximum in array
- **test144.dl**: Factorial calculation with loop
- **test145.dl**: String building with loop
- **test146.dl**: Nested data structure manipulation
- **test147.dl**: Function that processes array
- **test148.dl**: Conditional array construction
- **test149.dl**: Mixed type operations
- **test150.dl**: Complex conditional logic with recursion (Fibonacci)


