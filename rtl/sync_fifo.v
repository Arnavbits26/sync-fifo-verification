// Synchronous FIFO with registered read data.
//
// Semantics (the C++ reference model in tb/tb_fifo.cpp mirrors these exactly):
//   * A write is accepted when wr_en && !full   (evaluated before the clock edge)
//   * A read  is accepted when rd_en && !empty  (evaluated before the clock edge)
//   * A simultaneous read and write is allowed when the FIFO is neither full nor empty.
//     At full, only the read is accepted; at empty, only the write is accepted.
//   * rdata/rvalid are registered: they present the word read on the previous edge.
//
// Fault injection: compile with -DBUG_1 ... -DBUG_10 to introduce one deliberate
// design bug (used by scripts/run_regression.py to measure how well the testbench
// detects faults). With no BUG_ define this is the golden design.
module sync_fifo #(
    parameter DATA_W = 8,
    parameter ADDR_W = 3
) (
    input  wire                clk,
    input  wire                rst_n,
    input  wire                wr_en,
    input  wire [DATA_W-1:0]   wdata,
    input  wire                rd_en,
    output reg  [DATA_W-1:0]   rdata,
    output reg                 rvalid,
    output wire                full,
    output wire                empty,
    output reg  [ADDR_W:0]     count
);
    localparam DEPTH = 1 << ADDR_W;

    reg [DATA_W-1:0] mem [0:DEPTH-1];
    reg [ADDR_W-1:0] wptr, rptr;

`ifdef BUG_1
    assign full  = (count == DEPTH - 1);          // BUG_1: full flag asserts one entry early
`else
    assign full  = (count == DEPTH);
`endif
    assign empty = (count == 0);

`ifdef BUG_2
    wire do_wr = wr_en;                           // BUG_2: ignores full -> overflow
`elsif BUG_9
    wire do_wr = wr_en && (!full || rd_en);       // BUG_9: accepts a write at full if a read happens too
`else
    wire do_wr = wr_en && !full;
`endif

`ifdef BUG_3
    wire do_rd = rd_en;                           // BUG_3: ignores empty -> underflow
`else
    wire do_rd = rd_en && !empty;
`endif

    integer i;
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            wptr   <= 0;
            rptr   <= 0;
            count  <= 0;
            rdata  <= 0;
            rvalid <= 1'b0;
            for (i = 0; i < DEPTH; i = i + 1) mem[i] <= 0;
        end else begin
`ifdef BUG_7
            rvalid <= rd_en;                      // BUG_7: rvalid set on a read attempt at empty
`else
            rvalid <= do_rd;
`endif

`ifdef BUG_4
            if (do_wr && !do_rd) begin            // BUG_4: write dropped on a simultaneous read/write
`elsif BUG_10
            // BUG_10 (rare corner case): write dropped only when one exact data value (0xFF) arrives
            // during a simultaneous read/write while the FIFO holds DEPTH-1 entries.
            if (do_wr && !(do_rd && count == DEPTH - 1 && wdata == {DATA_W{1'b1}})) begin
`else
            if (do_wr) begin
`endif
`ifdef BUG_8
                mem[wptr] <= (wptr == DEPTH - 1) ? (wdata ^ {{(DATA_W-1){1'b0}}, 1'b1}) : wdata; // BUG_8: LSB flipped in last slot
`else
                mem[wptr] <= wdata;
`endif
                wptr <= wptr + 1'b1;
            end

            if (do_rd) begin
                rdata <= mem[rptr];
`ifdef BUG_6
                rptr <= (rptr == DEPTH - 1) ? {{(ADDR_W-1){1'b0}}, 1'b1} : rptr + 1'b1; // BUG_6: bad read-pointer wrap
`else
                rptr <= rptr + 1'b1;
`endif
            end

            case ({do_wr, do_rd})
                2'b10: count <= count + 1'b1;
                2'b01: count <= count - 1'b1;
`ifdef BUG_5
                2'b11: count <= count + 1'b1;     // BUG_5: count wrong on simultaneous read/write
`endif
                default: count <= count;
            endcase
        end
    end
endmodule
