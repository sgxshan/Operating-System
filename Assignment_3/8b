'''Constraint Satisfaction Routines
   A) class Variable

      This class allows one to define CSP variables.

      Currently no support for backtracking--i.e., can not remember
      and undo changes, so if one wants to implement backtracking
      search an extension is needed.

      On initialization the variable object can be given a name, and
      an original domain of values. This list of domain values can be
      added but NOT deleted from.
      
      To support GAC propagation, the class also maintains a
      current domain for the variable. Values pruned from the variable
      domain are removed from the current domain but not from the
      original domain.

      The current domain can be re-initialized at any point to be
      equal to the original domain.

    B) class Constraint

      This class allows one to define constraints specified by tables
      of satisfying assignments.

      On initialization the variables the constraint is over is
      specified (i.e. the scope of the constraint). This must be an
      ORDERED list of variables. This list of variables cannot be
      changed once the constraint object is created.

      Once initialized the constraint can be incrementally initialized
      with a list of satisfying tuples. Each tuple specifies a value
      for each variable in the constraint (in the same ORDER as the
      variables of the constraint were specified).

'''

class Variable:
    '''Class for defining CSP variables'''
    def __init__(self, name, domain=[]):
        '''Create a variable object, specifying its name (a
        string). Optionally specify the initial domain.
        '''
        self.name = name                #text name for variable
        self.dom = list(domain)         #Make a copy of passed domain
        self.curdom = list(domain)      #using list

    def add_domain_values(self, values):
        '''Add additional domain values to the domain'''
        for val in values: self.dom.append(val)
        for val in values: self.curdom.append(val)

    def value_index(self, value):
        '''Domain values need not be numbers, so return the index
           in the domain list of a variable value'''
        return self.dom.index(value)

    def prune_value(self, value):
        '''Remove value from current domain'''
        self.curdom.remove(value)

    def domain_size(self):
        '''Return the size of the domain'''
        return(len(self.dom))

    def domain(self):
        '''return the variable domain'''
        return(self.dom)

    def cur_domain(self):
        '''return the variable current domain'''
        return(self.curdom)

    def cur_domain_size(self):
        '''Return the size of the current domain'''
        return(len(self.curdom))

    def in_cur_domain(self, value):
        '''check if value is in current domain'''
        return(value in self.curdom)

    def __repr__(self):
        return("Variable \"{}\"".format(self.name))

    def print_var(self):
        '''Also print the variable domain and current domain'''
        print("Variable\"{}\": Dom = {}, CurDom = {}".format(self.name, self.dom, self.curdom))

class Constraint: 
    '''Class for defining constraints specified as lists of satisfying
       tuples. Note that the initial scope passed as a list of
       variable objects specifes an ordering over the tuples. That is, each
       tuple must be a ordered list of values, one for each variable
       in scope. For example, if the constaint scope is specified to be
       [v1, v2, v3], where each vi is a variable object then a
       satisfying tuple will have to be a list of three values. For
       example, the tuple [1, 2, 1] would specify the assignments
       v1=1, v2=2, v3=1---the ordering has to agree with the scope.'''

    def __init__(self, name, scope): 
        '''create a constraint object, specify the constraint name (a
        string) and its scope (an ORDERED list of variable
        objects). The list of satisfying tuples is specified later
        using add_satisfying_tuples'''

        self.scope = list(scope)
        self.name = name
        self.sat_tuples = []
        #The next object data item 'sup_tuples' will be used to help
        #support GAC propgation. It is a list of lists (like a matrix)
        #indexed by two integers [variable position in scope][value
        #position in variable domain] At this position of the matrix
        #will be stored a list of all supporting tuples for that variable
        #value pair. Initially, these lists are set to be empty by the
        #following code. NOTE that support tuple data structure is
        #space expensive, and for solving real CSP problems better
        #data structures would be required.
        self.sup_tuples = []
        for i in range(len(self.scope)): #i-th variable
            self.sup_tuples.append([])
            for j in range(self.scope[i].domain_size()): #j-th value
                self.sup_tuples[i].append([])

    def add_satisfying_tuples(self, tuples):
        '''Add list of satisfying tuple to the constraint.'''
        for t in tuples:
            self.sat_tuples.append(t)
            #now put t in as a support for all of the variable values in it
            i = 0
            for val in t:  #i'th val in tuple is a value for the i'th variable
                j = self.scope[i].value_index(val) #find value's index
                self.sup_tuples[i][j].append(t)    #add tuple on Vi=j support list
                i = i+1

    def tuple_is_valid(self, t):
        '''internal routine to test if a tuple contains values that are
           in the current domain of the variables. The constraint scope
           determines what variable each value in the tuple corresponds to'''
        i = 0
        for val in t:
             var = self.scope[i]
             if not var.in_cur_domain(val):
                return False
             i = i + 1
        return True

    def has_support(self, var, val):
        '''Test if a variable value pair has a supporting tuple (set 
           of assignments satisfying the constraint where each assignment
           is a value that is still in the corresponding variable's current
           domain'''
           
        i = self.scope.index(var)
        j = var.value_index(val)
        #it would be more efficient to work through the list of supporting tuples
        #from the end, popping tuples as they are found to be invalid. 
        #this way we would avoid processing the same invalid tuple twice.
        #However, if eventually we add support for backtracking (i.e., restoring
        #tuples to validity) this would make it more complex. So for now 
        #just do it the more time consuming but easier way.
        for t in self.sup_tuples[i][j]:
            if self.tuple_is_valid(t):
                return True
        return False

    def __repr__(self):
        return("Constraint \"{}{}\"".format(self.name,[var.name for var in self.scope]))

    def print_constraint(self):
        '''print basic information about the constraint'''
        print("Constraint {}: scope = {}".format(self.name,
                                                 [var.name for var in self.scope]))

    def print_constraint_all(self):
        '''print all of the information about the constraint (can be voluminous)'''
        self.print_constraint()
        print("Satisfying Tuples:")
        for t in self.sat_tuples:
            print("{}: {}".format(t, self.tuple_is_valid(t)))

        print("Supporting Tuples")
        for var in self.scope:
            for val in var.domain():
                print("  {} = {}: {}".format(var.name,
                                             val,
                                             self.sup_tuples[self.scope.index(var)][var.value_index(val)]))

