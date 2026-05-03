{$R+}
program chk( INPUT, OUTPUT );

type
    flagType = array[ 0..2 ] of boolean;

var
    flags : flagType;
    i : integer;

begin
    writeln( 'a' );

    for i := 0 to 10  do begin
        flags[ i ] := true;
        writeln( flags[ i ] );
    end;


    writeln( 'chk did not stop a write to unallocated memory!' );
end.
