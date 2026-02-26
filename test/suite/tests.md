# Test Coverage by Category

### Section 1: Variable Declarations (Tests 1-10)
Focus on variable declaration syntax and initialization.
- **test1.d**: Simple integer variable declaration
- **test2.d**: Simple real variable declaration
- **test3.d**: Simple string variable declaration
- **test4.d**: Simple boolean variable declaration
- **test5.d**: Variable declaration without initialization (none type)
- **test6.d**: Multiple variables in one declaration
- **test7.d**: Multiple variables with some uninitialized
- **test8.d**: Variable reassignment with same type
- **test9.d**: Variable type change (dynamic typing)
- **test10.d**: Variable shadowing in nested scopes

---

### Section 2: Integer Operations (Tests 11-25)
Comprehensive testing of arithmetic operations on integers and operator precedence.
- **test11.d**: Integer addition
- **test12.d**: Integer subtraction
- **test13.d**: Integer multiplication
- **test14.d**: Integer division (round down)
- **test15.d**: Unary plus on integer
- **test16.d**: Unary minus on integer
- **test17.d**: Integer comparison: less than
- **test18.d**: Integer comparison: greater than
- **test19.d**: Integer comparison: equal
- **test20.d**: Integer comparison: not equal
- **test21.d**: Integer comparison: less or equal
- **test22.d**: Integer comparison: greater or equal
- **test23.d**: Multiple integer operations in chain (precedence)
- **test24.d**: Integer with parentheses
- **test25.d**: Integer division by 1

---

### Section 3: Real Number Operations (Tests 26-35)
Testing floating-point arithmetic and mixed int/real operations.
- **test26.d**: Real addition
- **test27.d**: Real subtraction
- **test28.d**: Real multiplication
- **test29.d**: Real division
- **test30.d**: Integer plus real (implicit conversion)
- **test31.d**: Real plus integer (implicit conversion)
- **test32.d**: Real comparison
- **test33.d**: Real equality
- **test34.d**: Unary minus on real
- **test35.d**: Real with many decimal places

---

### Section 4: String Operations (Tests 36-42)
String manipulation and concatenation.
- **test36.d**: Simple string concatenation
- **test37.d**: String with single quotes
- **test38.d**: String concatenation with variable
- **test39.d**: Empty string
- **test40.d**: Multiple string concatenations
- **test41.d**: String comparison (equality)
- **test42.d**: Boolean AND operation

---

### Section 5: Boolean Operations (Tests 43-50)
Logical operations on boolean values.
- **test43.d**: Boolean AND operation
- **test44.d**: Boolean AND with false
- **test45.d**: Boolean OR operation
- **test46.d**: Boolean OR with both false
- **test47.d**: Boolean XOR operation
- **test48.d**: Boolean XOR with same values
- **test49.d**: Unary NOT on true
- **test50.d**: Unary NOT on false

---

### Section 6: Type Checking with 'is' Operator (Tests 51-60)
Type checking and type validation.
- **test51.d**: Type check - integer
- **test52.d**: Type check - real
- **test53.d**: Type check - string
- **test54.d**: Type check - boolean
- **test55.d**: Type check - none
- **test56.d**: Type check - array
- **test57.d**: Type check - tuple
- **test58.d**: Type check - function
- **test59.d**: Type mismatch check
- **test60.d**: Type check after assignment change

---

### Section 7: Array Operations (Tests 61-75)
Array creation, indexing, and manipulation.
- **test61.d**: Empty array
- **test62.d**: Array with integers
- **test63.d**: Array with mixed types
- **test64.d**: Array access by index (first element)
- **test65.d**: Array access (second element)
- **test66.d**: Array access (third element)
- **test67.d**: Array assignment/modification
- **test68.d**: Array with single element
- **test69.d**: Array concatenation
- **test70.d**: Nested arrays
- **test71.d**: Access nested array element
- **test72.d**: Array type check
- **test73.d**: Associative array - sparse indices
- **test74.d**: Array modification
- **test75.d**: Array with function references

---

### Section 8: Tuple Operations (Tests 76-85)
Tuple creation, element access, and manipulation.
- **test76.d**: Simple tuple with named elements
- **test77.d**: Tuple element access by name
- **test78.d**: Tuple with unnamed elements
- **test79.d**: Tuple element access by index
- **test80.d**: Mixed named and unnamed tuple elements
- **test81.d**: Tuple type check
- **test82.d**: Tuple concatenation
- **test83.d**: Tuple with nested values
- **test84.d**: Tuple with mixed types
- **test85.d**: Tuple access by integer index

---

### Section 9: Conditional Statements (Tests 86-92)
If/then/else and short if form testing.
- **test86.d**: Simple if statement
- **test87.d**: If with else
- **test88.d**: If short form
- **test89.d**: If short form with false condition
- **test90.d**: Nested if statements
- **test91.d**: If with boolean variable
- **test92.d**: If with comparison

---

### Section 10: Loop Constructs (Tests 93-100)
For loops, while loops, and loop exit mechanisms.
- **test93.d**: For loop with range
- **test94.d**: For loop with variable
- **test95.d**: While loop
- **test96.d**: Loop with exit
- **test97.d**: For loop iterating array
- **test98.d**: Nested loops
- **test99.d**: Loop with multiple statements
- **test100.d**: Infinite loop with counter exit

---

### Section 11: Function Operations (Tests 101-120)
Function definition, parameters, recursion, and higher-order functions.
- **test101.d**: Function with one parameter
- **test102.d**: Function with multiple parameters
- **test103.d**: Function with body (multi-statement)
- **test104.d**: Function returning array
- **test105.d**: Function returning tuple
- **test106.d**: Function returning function (higher-order)
- **test107.d**: Recursive function (factorial)
- **test108.d**: Function with no explicit return
- **test109.d**: Function stored in array
- **test110.d**: Higher order function (function as argument)
- **test111.d**: Function with conditional logic
- **test112.d**: Function assigning to variable in body
- **test113.d**: Multiple function calls (composition)
- **test114.d**: Function in tuple
- **test115.d**: Return with value

---

### Section 12: Return Statements (Tests 116-120)
Testing various return statement scenarios.
- **test116.d**: Return without value
- **test117.d**: Early return
- **test118.d**: Return from nested block
- **test119.d**: Return expression

---

### Section 13: Print Statements (Tests 121-130)
Print statement with various types and expressions.
- **test121.d**: Print single value
- **test122.d**: Print multiple values
- **test123.d**: Print string
- **test124.d**: Print variable
- **test125.d**: Print expression
- **test126.d**: Print array
- **test127.d**: Print tuple
- **test128.d**: Print boolean
- **test129.d**: Print multiple strings
- **test130.d**: Print with variable modifications

---

### Section 14: Complex Expressions (Tests 131-140)
Operator precedence, complex expressions, and mixed operations.
- **test131.d**: Mixed arithmetic with precedence
- **test132.d**: Boolean logic precedence
- **test133.d**: Comparison chaining simulation
- **test134.d**: Expression with parentheses
- **test135.d**: Type conversion in expression
- **test136.d**: Array element in expression
- **test137.d**: Tuple element in expression
- **test138.d**: Function call in expression
- **test139.d**: Complex nested expression
- **test140.d**: Logical operators combination

---

### Section 15: Complex Integrated Tests (Tests 141-150)
Real-world algorithms and complex scenarios combining multiple features.
- **test141.d**: Sum array elements using loop
- **test142.d**: Count loop iterations
- **test143.d**: Find maximum in array
- **test144.d**: Factorial calculation with loop
- **test145.d**: String building with loop
- **test146.d**: Nested data structure manipulation
- **test147.d**: Function that processes array
- **test148.d**: Conditional array construction
- **test149.d**: Mixed type operations
- **test150.d**: Complex conditional logic with recursion (Fibonacci)


